#include "engine/EngineContext.h"
#include "engine/components/AnimatedModel.h"
#include "game/SectorScriptBindings.h"
#include "game/save/GameSaveRuntime.h"
#include "sector_demo/SectorBoxSweep.h"
#include "sector_demo/SectorPropDragging.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorSceneRuntime.h"
#include "sector_demo/SectorUseInteraction.h"
#include <cassert>
#include <cmath>

namespace
{
bool Near(float a, float b) { return std::abs(a - b) < 0.002f; }
game::SectorTopologyMap Room()
{
    using namespace game;
    SectorTopologyMap map;
    map.vertices = {{1, 0, 0}, {2, 1280, 0}, {3, 1280, 1280}, {4, 0, 1280}};
    map.lineDefs = {{1, 1, 2, 1, -1}, {2, 2, 3, 2, -1}, {3, 3, 4, 3, -1}, {4, 4, 1, 4, -1}};
    for (int i = 1; i <= 4; ++i)
    {
        SectorTopologySideDef s;
        s.id = i;
        s.lineDefId = i;
        s.sectorId = 1;
        s.side = SectorTopologySideKind::Front;
        map.sideDefs.push_back(s);
    }
    SectorTopologySector sector;
    sector.id = 1;
    sector.floorZ = 0;
    sector.ceilingZ = 32;
    map.sectors.push_back(sector);
    return map;
}
struct Fixture
{
    engine::EngineContext context;
    game::SectorTopologyMap map = Room();
    game::SectorCollisionWorld collision;
    game::SectorRuntimeObjectState objects;
    game::SectorPropDragSession session;
    game::SectorFpsControllerState player;
    game::SectorFpsControllerConfig config;
    engine::Entity entity;
    Fixture()
    {
        using namespace game;
        assert(collision.BuildFromTopology(map));
        context.world.ReserveEntities(8);
        context.world.ReserveComponentTypes(8);
        context.world.ReserveComponent<SectorPropDrag>(4);
        context.world.ReserveComponent<SectorObjectTransform>(8);
        context.world.ReserveComponent<SectorStaticModelCollider>(8);
        context.world.ReserveComponent<SectorDynamicModel>(4);
        context.world.ReserveComponent<SectorObject>(8);
        context.world.ReserveComponent<engine::AnimatedModelInstance>(4);
        context.world.ReserveComponent<SectorItem>(4);
        context.world.LockComponentRegistration();
        SectorAuthoringPath path;
        path.editorId = 1;
        path.id = "cabinet_path";
        path.waypoints = {{1, 256, 256}, {2, 640, 256}, {3, 640, 768}};
        map.paths.push_back(CompileSectorPath(path));
        entity = context.world.CreateEntity();
        SectorPropDrag drag;
        drag.settings.pathEditorId = 1;
        drag.settings.speedWorld = 1;
        context.world.Add(entity, drag);
        context.world.Add(entity, SectorObjectTransform{{2, 0, 2}, 0.4f});
        SectorStaticModelCollider box;
        box.placedObjectId = 1;
        box.center = {2, 2};
        box.halfExtents = {0.4f, 0.4f};
        box.top = 1.5f;
        box.resolved = true;
        context.world.Add(entity, box);
        SectorDynamicModel model;
        model.placedObjectId = 1;
        model.useTitle = "Cabinet";
        model.useDistance = 3;
        context.world.Add(entity, model);
        context.world.Add(entity, SectorObject{});
        objects.staticModelColliders.reserve(8);
        objects.physicalModelColliders.reserve(8);
        objects.dynamicModelColliders.reserve(8);
        RefreshSectorDragColliders(context, objects);
        player.feetPosition = {1, 0, 2};
        player.grounded = true;
        player.currentSectorId = 1;
    }
    void Move(bool forward, float dt)
    {
        game::SectorFpsControllerInput input;
        input.moveForward = forward;
        input.moveBackward = !forward;
        assert(game::UpdateSectorPropDrag(context, map, objects, collision, {}, session, player,
                                          config, input, dt));
        assert(!input.moveForward && !input.moveBackward);
    }
};
void TravelAndRelease()
{
    using namespace game;
    Fixture f;
    assert(BeginSectorPropDrag(f.context, f.map, f.session, f.entity, f.player));
    f.Move(true, 4);
    auto &t = f.context.world.Get<SectorObjectTransform>(f.entity);
    auto &drag = f.context.world.Get<SectorPropDrag>(f.entity);
    assert(Near(t.position.x, 5) && Near(t.position.z, 2.85f) && Near(drag.distanceWorld, 3.85f));
    assert(Near(f.player.feetPosition.x, 4) && Near(f.player.feetPosition.z, 2.85f) &&
           Near(t.yawRadians, 0.4f));
    f.Move(false, 2);
    assert(Near(t.position.x, 4.3f) && Near(t.position.z, 2));
    f.Move(true, 100);
    assert(Near(drag.distanceWorld, 7) && Near(t.position.z, 6));
    f.Move(false, 100);
    assert(Near(drag.distanceWorld, 0) && Near(t.position.x, 2));
    f.Move(true, 0.37f);
    EndSectorPropDrag(f.context, f.session, true);
    assert(engine::IsNull(f.session.entity) && Near(drag.distanceWorld, 0.22f));
    assert(BeginSectorPropDrag(f.context, f.map, f.session, f.entity, f.player));
    f.Move(false, 1);
    assert(Near(drag.distanceWorld, 0));
    f.player.grounded = false;
    SectorFpsControllerInput input;
    assert(!UpdateSectorPropDrag(f.context, f.map, f.objects, f.collision, {}, f.session, f.player,
                                 f.config, input, 1));
    assert(engine::IsNull(f.session.entity));
}
void Coast(Fixture &f, float dt)
{
    game::SectorFpsControllerInput input;
    game::UpdateSectorPropDrag(f.context, f.map, f.objects, f.collision, {}, f.session, f.player,
                               f.config, input, dt);
}
void AccelerationAndBraking()
{
    using namespace game;
    Fixture f;
    assert(BeginSectorPropDrag(f.context, f.map, f.session, f.entity, f.player));
    auto &distance = f.context.world.Get<SectorPropDrag>(f.entity).distanceWorld;
    f.Move(true, 0.15f);
    assert(Near(f.session.velocityWorld, 0.5f) && Near(distance, 0.0375f));
    f.Move(true, 0.15f);
    assert(Near(f.session.velocityWorld, 1) && Near(distance, 0.15f));
    Coast(f, 0.1f);
    assert(Near(f.session.velocityWorld, 0.5f) && Near(distance, 0.225f));
    Coast(f, 0.1f);
    assert(Near(f.session.velocityWorld, 0) && Near(distance, 0.25f));
    Coast(f, 1);
    assert(Near(distance, 0.25f));
    f.Move(true, 0.3f);
    f.Move(false, 0.1f);
    assert(f.session.velocityWorld > 0); // reversal first brakes the old motion
    f.Move(false, 0.25f);
    assert(Near(f.session.velocityWorld, -0.5f));
    const float released = distance;
    EndSectorPropDrag(f.context, f.session, true);
    Coast(f, 1);
    assert(Near(distance, released) && f.session.velocityWorld == 0);

    Fixture shortPath;
    auto &path = shortPath.map.paths.front();
    path.points = {{2, 2}, {2.1f, 2}};
    path.distances = {0, 0.1f};
    path.length = 0.1f;
    assert(BeginSectorPropDrag(shortPath.context, shortPath.map, shortPath.session,
                               shortPath.entity, shortPath.player));
    shortPath.Move(true, 0.25f);
    assert(shortPath.session.velocityWorld > 0 && shortPath.session.velocityWorld < 0.5f);
    shortPath.Move(true, 1);
    assert(Near(shortPath.context.world.Get<SectorPropDrag>(shortPath.entity).distanceWorld, 0.1f));
    assert(shortPath.session.velocityWorld == 0);
    shortPath.Move(false, 1);
    assert(Near(shortPath.context.world.Get<SectorPropDrag>(shortPath.entity).distanceWorld, 0));

    Fixture blocked;
    assert(BeginSectorPropDrag(blocked.context, blocked.map, blocked.session, blocked.entity,
                               blocked.player));
    blocked.Move(true, 0.3f);
    SectorStaticModelCollider obstacle;
    obstacle.placedObjectId = 2;
    obstacle.center = {2.6f, 2};
    obstacle.halfExtents = {0.025f, 0.4f};
    obstacle.bottom = 0;
    obstacle.top = 2;
    blocked.objects.physicalModelColliders.push_back(obstacle);
    Coast(blocked, 0.2f);
    assert(blocked.session.velocityWorld == 0);
    assert(blocked.context.world.Get<SectorPropDrag>(blocked.entity).distanceWorld < 0.18f);
}
void FrameRateIndependentMotion()
{
    using namespace game;
    for (int fps : {30, 60, 144})
    {
        Fixture f;
        assert(BeginSectorPropDrag(f.context, f.map, f.session, f.entity, f.player));
        for (int i = 0; i < 4 * fps; ++i)
            f.Move(true, 1.0f / fps);
        auto &distance = f.context.world.Get<SectorPropDrag>(f.entity).distanceWorld;
        assert(Near(distance, 3.85f) && Near(f.session.velocityWorld, 1));
        // Exact duration independent of whether 0.2 seconds is a whole frame count.
        for (int i = 0; i < fps; ++i)
            Coast(f, 0.2f / fps);
        assert(Near(distance, 3.95f) && Near(f.session.velocityWorld, 0));
        for (int i = 0; i < fps; ++i)
            f.Move(false, 1.0f / fps);
        assert(Near(distance, 3.1f) && Near(f.session.velocityWorld, -1));
        f.Move(true, 100);
        assert(Near(distance, 7) && f.session.velocityWorld == 0);
    }
}
void LimitedMouseLook()
{
    using namespace game;
    Fixture f;
    PlayerCameraApplicationSettings settings;
    settings.smoothingStrength = 0;
    settings.maxTurnSpeedDegreesPerSecond = 0;
    SectorFpsControllerInput input;
    input.mouseLookEnabled = true;
    const auto look = [&](Vector2 mouse, float dt = 0.05f)
    {
        input.mouseDelta = mouse;
        UpdateSectorPropDragMouseLook(f.context.world, f.session, f.player, f.config, settings,
                                      input, dt);
    };
    f.player.yawRadians = 20 * DEG2RAD;
    assert(BeginSectorPropDrag(f.context, f.map, f.session, f.entity, f.player));
    look({});
    assert(f.player.yawRadians > 0 && f.player.yawRadians < 20 * DEG2RAD);
    look({});
    look({});
    look({});
    const float centerPitch = std::atan2(0.75f - f.config.eyeHeight, 1.0f);
    assert(Near(f.player.yawRadians, 0) && Near(f.player.pitchRadians, centerPitch));
    look({100000, -100000});
    assert(Near(f.player.yawRadians, 40 * DEG2RAD));
    assert(Near(f.player.pitchRadians, centerPitch + 25 * DEG2RAD));
    look({-100000, 100000});
    assert(Near(f.player.yawRadians, -40 * DEG2RAD));
    assert(Near(f.player.pitchRadians, centerPitch - 25 * DEG2RAD));
    const float heldYaw = f.player.yawRadians, heldPitch = f.player.pitchRadians;
    for (int i = 0; i < 20; ++i)
        look({});
    assert(Near(f.player.yawRadians, heldYaw) && Near(f.player.pitchRadians, heldPitch));
    f.Move(true, 4); // follows through a bend without rotating the grabbed side
    look({});
    assert(Near(f.player.yawRadians, heldYaw) && Near(f.player.pitchRadians, heldPitch));
    settings.smoothingStrength = 0.5f;
    look({-100000, 0});
    assert(f.player.mouseLook.angularVelocity.x == 0);
    look({10, 0});
    assert(f.player.yawRadians > -40 * DEG2RAD);
    const float releasedYaw = f.player.yawRadians;
    EndSectorPropDrag(f.context, f.session, true);
    look({});
    assert(Near(f.player.yawRadians, releasedYaw));
    settings.smoothingStrength = 0;
    look({1000, 0});
    assert(std::abs(f.player.yawRadians - releasedYaw) > 40 * DEG2RAD);

    // The prop is now west of the player; centering must cross +/-pi the short way.
    Fixture wrap;
    wrap.player.feetPosition = {3, 0, 2};
    wrap.player.yawRadians = -PI + 0.1f;
    assert(BeginSectorPropDrag(wrap.context, wrap.map, wrap.session, wrap.entity, wrap.player));
    input.mouseDelta = {};
    UpdateSectorPropDragMouseLook(wrap.context.world, wrap.session, wrap.player, wrap.config,
                                  settings, input, 0.05f);
    assert(wrap.player.yawRadians < -PI + 0.1f && wrap.player.yawRadians >= -PI);
    for (int i = 0; i < 4; ++i)
        UpdateSectorPropDragMouseLook(wrap.context.world, wrap.session, wrap.player, wrap.config,
                                      settings, input, 0.05f);
    assert(Near(wrap.player.yawRadians, -PI));
}
void BothBodiesBlock()
{
    using namespace game;
    for (bool blockPlayer : {false, true})
    {
        Fixture f;
        assert(BeginSectorPropDrag(f.context, f.map, f.session, f.entity, f.player));
        SectorStaticModelCollider obstacle;
        obstacle.placedObjectId = 2;
        obstacle.center = blockPlayer ? Vector2{1, 3} : Vector2{4, 2};
        obstacle.halfExtents = {0.025f, 0.025f};
        obstacle.top = 2;
        obstacle.resolved = true;
        f.objects.physicalModelColliders.push_back(obstacle);
        if (blockPlayer)
        {
            // A turn makes the player run into an obstacle the prop clears.
            obstacle.center = {4, 3};
            f.objects.physicalModelColliders.back() = obstacle;
            const auto e = f.context.world.CreateEntity();
            f.context.world.Add(e, obstacle);
        }
        f.Move(true, 100);
        const auto &t = f.context.world.Get<SectorObjectTransform>(f.entity);
        assert(blockPlayer ? t.position.z < 3 : t.position.x < 4);
        assert(Near(t.position.x - f.player.feetPosition.x, 1));
        assert(Near(t.position.z - f.player.feetPosition.z, 0));
    }
}
void NativeUseAndHiddenItem()
{
    using namespace game;
    Fixture f;
    f.context.world.Add(f.entity, engine::AnimatedModelInstance{});
    const auto item = f.context.world.CreateEntity();
    f.context.world.Add(item, SectorObjectTransform{{3, 0.5f, 2}});
    SectorItem pickup;
    pickup.takeDistance = 5;
    pickup.title = "Key";
    f.context.world.Add(item, pickup);
    const auto target = FindSectorUseTarget(f.context.world, nullptr, {0.8f, 0.6f, 2}, {1, 0, 0},
                                            &f.collision, false, &f.map);
    assert(target.entity == f.entity && target.draggable);
    auto &box = f.context.world.Get<SectorStaticModelCollider>(f.entity);
    box.center.y = 5;
    f.context.world.Get<SectorObjectTransform>(f.entity).position.z = 5;
    const auto revealed = FindSectorUseTarget(f.context.world, nullptr, {0.8f, 0.6f, 2}, {1, 0, 0},
                                              &f.collision, false, &f.map);
    assert(revealed.entity == item);
}
void ContinuousSweeps()
{
    using namespace game;
    Vector2 thinWall[]{{4, 0}, {4, 5}};
    const float hit =
        SweepSectorBoxPolygon({2, 2}, {1, 0}, {0, 1}, {0.5f, 0.5f}, {100, 0}, thinWall, 2);
    assert(hit > 0 && hit < 0.016f);
    assert(Near(
        SweepSectorBoxPolygon({3.5f, 2}, {1, 0}, {0, 1}, {0.5f, 0.5f}, {-1, 0}, thinWall, 2), 1));
    SectorCollisionWorld world;
    auto map = Room();
    assert(world.BuildFromTopology(map));
    const float wall =
        world.SweepFlatBox({2, 2}, {1, 0}, {0, 1}, {0.5f, 0.5f}, 0, 1.5f, 0, {100, 0});
    assert(wall > 0 && wall < 0.076f);
    assert(world.SweepFlatBox({2, 2}, {1, 0}, {0, 1}, {0.5f, 0.5f}, 0, 1.5f, 1, {1, 0}) == 0);
}
void SavedProgressUsesStableIdentity()
{
    using namespace game;
    Fixture f;
    SectorSceneRuntime scene;
    auto &objects = scene.RuntimeObjects();
    objects = f.objects;
    objects.placedObjectEntities.push_back({1, f.entity});
    objects.objectSectorLookupWorld = f.collision;
    objects.objectSectorLookupWorldValid = true;
    SectorPlacedRuntimeObject authored;
    authored.id = 1;
    authored.kind = "dynamic_model";
    authored.dynamicModel.drag.pathEditorId = 1;
    authored.dynamicModel.instanceId = "cabinet";
    f.map.runtimeObjects.push_back(authored);
    f.context.world.Get<SectorDynamicModel>(f.entity).instanceId = "cabinet";
    auto &drag = f.context.world.Get<SectorPropDrag>(f.entity);
    drag.distanceWorld = 4.25f;
    SectorScriptHost host;
    const auto saved =
        CaptureGameSaveLevelState(f.context.world, f.context.assets, f.map, objects, host, "room");
    assert(saved.props.size() == 1 && saved.props[0].dragPathEditorId == 1);
    assert(Near(saved.props[0].dragDistanceWorld, 4.25f));
    drag.distanceWorld = 0;
    ApplyGameSaveLevelRuntimeState(f.context.world, f.context.assets, scene, f.map, host, saved);
    const auto &transform = f.context.world.Get<SectorObjectTransform>(f.entity);
    assert(Near(drag.distanceWorld, 4.25f) && Near(transform.position.x, 5) &&
           Near(transform.position.z, 3.25f));
    auto altered = saved;
    altered.props[0].dragDistanceWorld = 100;
    ApplyGameSaveLevelRuntimeState(f.context.world, f.context.assets, scene, f.map, host, altered);
    assert(Near(drag.distanceWorld, 7));
    altered.props[0].dragPathEditorId = 99;
    drag.distanceWorld = 0;
    ApplyGameSaveLevelRuntimeState(f.context.world, f.context.assets, scene, f.map, host, altered);
    assert(Near(drag.distanceWorld, 0));
}

} // namespace
void RunSectorPropDraggingTests()
{
    AccelerationAndBraking();
    FrameRateIndependentMotion();
    LimitedMouseLook();
    SavedProgressUsesStableIdentity();
    TravelAndRelease();
    BothBodiesBlock();
    NativeUseAndHiddenItem();
    ContinuousSweeps();
}
