#include "sector_demo/SectorRuntimeObjects.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"
#include "util/json.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

using Json = nlohmann::ordered_json;

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(float actual, float expected, float epsilon = 0.00001f)
{
    return std::fabs(actual - expected) <= epsilon;
}

bool Near(Vector3 actual, Vector3 expected, float epsilon = 0.00001f)
{
    return Near(actual.x, expected.x, epsilon)
            && Near(actual.y, expected.y, epsilon)
            && Near(actual.z, expected.z, epsilon);
}

bool Near(Vector2 actual, Vector2 expected, float epsilon = 0.00001f)
{
    return Near(actual.x, expected.x, epsilon)
            && Near(actual.y, expected.y, epsilon);
}

bool LoadJsonFile(const char* path, Json& outJson)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    file >> outJson;
    return !file.fail();
}

void AddSide(
        game::SectorTopologyMap& map,
        int sideId,
        int lineId,
        game::SectorTopologySideKind side,
        int sectorId)
{
    game::SectorTopologySideDef sideDef;
    sideDef.id = sideId;
    sideDef.lineDefId = lineId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    map.sideDefs.push_back(sideDef);
}

game::SectorTopologySector Sector(int id)
{
    game::SectorTopologySector sector;
    sector.id = id;
    sector.floorZ = 0.0f;
    sector.ceilingZ = 32.0f;
    return sector;
}

void AddSectorLoop(
        game::SectorTopologyMap& map,
        int sectorId,
        const std::vector<std::pair<game::SectorCoord, game::SectorCoord>>& points)
{
    std::vector<int> vertexIds;
    for (const auto& point : points) {
        const int vertexId = game::AllocateSectorTopologyVertexId(map);
        map.vertices.push_back(game::SectorTopologyVertex{vertexId, point.first, point.second});
        vertexIds.push_back(vertexId);
    }

    for (size_t i = 0; i < vertexIds.size(); ++i) {
        const int lineId = game::AllocateSectorTopologyLineDefId(map);
        const int sideId = game::AllocateSectorTopologySideDefId(map);
        map.lineDefs.push_back(game::SectorTopologyLineDef{
                lineId,
                vertexIds[i],
                vertexIds[(i + 1) % vertexIds.size()],
                sideId,
                -1
        });
        AddSide(map, sideId, lineId, game::SectorTopologySideKind::Front, sectorId);
    }
}

game::SectorTopologyMap MakeSquareMap()
{
    game::SectorTopologyMap map;
    map.sectors.push_back(Sector(10));
    AddSectorLoop(map, 10, {{0, 0}, {64, 0}, {64, 64}, {0, 64}});
    return map;
}

game::SectorTopologyMap MakeDoorPortalMap()
{
    game::SectorTopologyMap map;
    map.vertices = {
            {1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64},
            {5, 128, 0}, {6, 128, 64}
    };
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, 8},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 3, 7, -1}
    };
    AddSide(map, 1, 1, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 2, game::SectorTopologySideKind::Back, 20);

    game::SectorTopologySector front = Sector(10);
    front.floorZ = 1.0f;
    front.ceilingZ = 16.0f;
    map.sectors.push_back(front);

    game::SectorTopologySector back = Sector(20);
    back.floorZ = 4.0f;
    back.ceilingZ = 20.0f;
    map.sectors.push_back(back);
    return map;
}

game::SectorPlacedDoor MakeDoorOnPortal()
{
    game::SectorPlacedDoor door;
    door.anchor.lineDefId = 2;
    door.anchor.frontSectorId = 10;
    door.anchor.backSectorId = 20;
    door.anchor.frontSideDefId = 2;
    door.anchor.backSideDefId = 8;
    door.anchor.endpointAX = 64;
    door.anchor.endpointAY = 0;
    door.anchor.endpointBX = 64;
    door.anchor.endpointBY = 64;
    return door;
}

game::SectorBakedObjectLightProbeRuntimeData MakeProbeRuntimeData()
{
    game::SectorBakedObjectLightProbeRuntimeData probes;

    game::SectorBakedObjectLightProbe probe;
    probe.sectorId = 10;
    probe.position = Vector3{2.0f, 1.0f, 2.0f};
    for (Vector3& face : probe.ambientCube) {
        face = Vector3{0.8f, 0.25f, 0.1f};
    }
    probes.probes.push_back(probe);

    game::SectorBakedObjectLightProbeSectorRange range;
    range.sectorId = 10;
    range.begin = 0;
    range.count = 1;
    probes.sectorRanges.push_back(range);

    return probes;
}

void TestResolveSectorDoorAnchorValidPortal()
{
    const game::SectorTopologyMap map = MakeDoorPortalMap();
    const game::SectorPlacedDoor door = MakeDoorOnPortal();

    const game::SectorResolvedDoorAnchor resolved = game::ResolveSectorDoorAnchor(map, door);

    Check(resolved.valid, "valid two-sided door portal resolves");
    Check(resolved.diagnostic.empty(), "valid door portal has no diagnostic");
    Check(resolved.lineDefId == 2
                  && resolved.frontSectorId == 10
                  && resolved.backSectorId == 20
                  && resolved.frontSideDefId == 2
                  && resolved.backSideDefId == 8,
          "resolved door anchor preserves stable IDs");
    Check(Near(resolved.endpointA, game::SectorCoordToWorldPosition2(64, 0))
                  && Near(resolved.endpointB, game::SectorCoordToWorldPosition2(64, 64)),
          "resolved door anchor uses current linedef endpoints");
    Check(Near(resolved.tangent, Vector2{0.0f, 1.0f})
                  && Near(resolved.normal, Vector2{1.0f, 0.0f}),
          "resolved door anchor tangent and front-to-back normal are deterministic");
    Check(Near(resolved.openBottom, game::SectorAuthoringToWorldDistance(4.0f))
                  && Near(resolved.openTop, game::SectorAuthoringToWorldDistance(16.0f)),
          "resolved door anchor vertical opening uses overlapping sector heights");
    Check(Near(resolved.portalWidth, game::SectorCoordDistanceToWorldDistance(64.0))
                  && Near(resolved.portalHeight, game::SectorAuthoringToWorldDistance(12.0f))
                  && Near(resolved.width, resolved.portalWidth)
                  && Near(resolved.height, resolved.portalHeight),
          "resolved door anchor supplies portal-derived default dimensions");
}

void TestResolveSectorDoorAnchorRejectsOneSidedWall()
{
    const game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.anchor.lineDefId = 1;
    door.anchor.frontSideDefId = 1;
    door.anchor.backSideDefId = 8;

    const game::SectorResolvedDoorAnchor resolved = game::ResolveSectorDoorAnchor(map, door);

    Check(!resolved.valid, "one-sided door anchor is rejected");
    Check(resolved.diagnostic.find("two-sided portal") != std::string::npos,
          "one-sided door anchor reports portal diagnostic");
}

void TestResolveSectorDoorAnchorRejectsSectorMismatch()
{
    const game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.anchor.backSectorId = 30;

    const game::SectorResolvedDoorAnchor resolved = game::ResolveSectorDoorAnchor(map, door);

    Check(!resolved.valid, "door anchor with changed sector pair is rejected");
    Check(resolved.diagnostic.find("sector pair") != std::string::npos,
          "door anchor sector mismatch reports diagnostic");
}

void TestResolveSectorDoorAnchorRejectsZeroHeightOpening()
{
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::FindSectorTopologySector(map, 20)->floorZ = 16.0f;
    const game::SectorPlacedDoor door = MakeDoorOnPortal();

    const game::SectorResolvedDoorAnchor resolved = game::ResolveSectorDoorAnchor(map, door);

    Check(!resolved.valid, "door anchor with zero-height opening is rejected");
    Check(resolved.diagnostic.find("vertical opening") != std::string::npos,
          "door anchor zero-height opening reports diagnostic");
}

void TestResolveSectorDoorAnchorUsesAuthoredDimensionsWhenPresent()
{
    const game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.width = 2.0f;
    door.height = 1.5f;

    const game::SectorResolvedDoorAnchor resolved = game::ResolveSectorDoorAnchor(map, door);

    Check(resolved.valid
                  && Near(resolved.portalWidth, game::SectorCoordDistanceToWorldDistance(64.0))
                  && Near(resolved.portalHeight, game::SectorAuthoringToWorldDistance(12.0f))
                  && Near(resolved.width, 2.0f)
                  && Near(resolved.height, 1.5f),
          "resolved door anchor preserves explicit authored dimensions");
}

void TestSectorRuntimeObjectComponentsIterateAndDestroy()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);

    const engine::Entity object = world.CreateEntity();
    game::SectorObjectTransform transform;
    transform.position = Vector3{1.0f, 2.0f, 3.0f};
    transform.yawRadians = 0.5f;
    world.Add(object, transform);

    game::SectorObject sectorObject;
    sectorObject.currentSectorId = 12;
    sectorObject.visible = true;
    world.Add(object, sectorObject);
    world.Add(object, game::SectorObjectLighting{});
    world.Add(object, game::SectorBillboardSprite{});
    world.Add(object, game::SectorBillboardDirectionalClips{});

    game::SectorBillboardAnimator animator;
    animator.animationId = "test.animation";
    world.Add(object, animator);

    int visited = 0;
    world.ForEach<
            game::SectorObjectTransform,
            game::SectorObject,
            game::SectorObjectLighting,
            game::SectorBillboardSprite,
            game::SectorBillboardDirectionalClips,
            game::SectorBillboardAnimator>(
            [&](engine::Entity entity,
                    game::SectorObjectTransform& objectTransform,
                    game::SectorObject& objectState,
                    game::SectorObjectLighting& lighting,
                    game::SectorBillboardSprite& sprite,
                    game::SectorBillboardDirectionalClips& directionalClips,
                    game::SectorBillboardAnimator& spriteAnimator) {
                Check(entity == object, "runtime object iteration returns the created entity");
                Check(objectTransform.position.x == 1.0f
                              && objectTransform.position.y == 2.0f
                              && objectTransform.position.z == 3.0f,
                        "runtime object transform stores world position");
                Check(objectTransform.yawRadians == 0.5f, "runtime object transform stores yaw");
                Check(objectState.currentSectorId == 12, "runtime object stores current sector id");
                Check(objectState.visible, "runtime object stores visibility flag");
                Check(!lighting.baked.valid, "runtime object lighting starts without a valid baked sample");
                Check(engine::IsNull(sprite.animation), "billboard sprite starts without an animation handle");
                Check(sprite.clipIndex == engine::InvalidSpriteClipIndex,
                        "billboard sprite starts without a resolved clip");
                Check(engine::IsNull(sprite.texture), "billboard sprite starts without a texture handle");
                Check(sprite.sizeWorld.x == 1.0f && sprite.sizeWorld.y == 1.0f,
                        "billboard sprite stores world size");
                Check(sprite.originNormalized.x == 0.5f && sprite.originNormalized.y == 1.0f,
                        "billboard sprite stores normalized bottom-center origin");
                Check(sprite.alphaCutoff == game::kSectorBillboardDefaultAlphaCutoff
                              && sprite.alphaCutoff == 0.5f,
                        "billboard sprite default alpha cutoff is 0.5");
                Check(sprite.visible, "billboard sprite stores visibility flag");
                Check(directionalClips.front == engine::InvalidSpriteClipIndex
                              && directionalClips.back == engine::InvalidSpriteClipIndex
                              && directionalClips.left == engine::InvalidSpriteClipIndex
                              && directionalClips.right == engine::InvalidSpriteClipIndex,
                        "billboard directional clips start unresolved");
                Check(!directionalClips.resolved && !directionalClips.usedFallback,
                        "billboard directional clips start without fallback state");
                Check(spriteAnimator.animationId == "test.animation",
                        "billboard animator stores data-driven animation id");
                Check(spriteAnimator.timeSeconds == 0.0f,
                        "billboard animator starts at time zero");
                Check(spriteAnimator.playing && spriteAnimator.loop && !spriteAnimator.finished,
                        "billboard animator stores playback state");
                ++visited;
            });
    Check(visited == 1, "runtime object iteration visits one matching entity");

    world.DestroyLater(object);
    world.FlushDestroyedEntities();
    Check(!world.IsAlive(object), "runtime object destruction flush retires entity");

    int visitedAfterDestroy = 0;
    world.ForEach<game::SectorObjectTransform, game::SectorObject>(
            [&](engine::Entity, game::SectorObjectTransform&, game::SectorObject&) {
                ++visitedAfterDestroy;
            });
    Check(visitedAfterDestroy == 0, "destroyed runtime object is removed from component iteration");
}

void TestSectorBillboardFrameUvsUseSourceRectangle()
{
    const game::SectorBillboardFrameUvs uvs = game::BuildSectorBillboardFrameUvs(
            Rectangle{32.0f, 16.0f, 8.0f, 24.0f},
            128,
            64);

    Check(Near(uvs.topLeft.x, 0.25f) && Near(uvs.topLeft.y, 0.25f),
            "billboard frame UV top-left uses source rectangle origin");
    Check(Near(uvs.topRight.x, 0.3125f) && Near(uvs.topRight.y, 0.25f),
            "billboard frame UV top-right uses source rectangle width");
    Check(Near(uvs.bottomRight.x, 0.3125f) && Near(uvs.bottomRight.y, 0.625f),
            "billboard frame UV bottom-right uses source rectangle height");
    Check(Near(uvs.bottomLeft.x, 0.25f) && Near(uvs.bottomLeft.y, 0.625f),
            "billboard frame UV bottom-left uses source rectangle height");
}

void TestSectorBillboardFrameUvsPreserveFlippedSourceSigns()
{
    const game::SectorBillboardFrameUvs uvs = game::BuildSectorBillboardFrameUvs(
            Rectangle{40.0f, 40.0f, -8.0f, -16.0f},
            128,
            64);

    Check(Near(uvs.topLeft.x, 0.3125f) && Near(uvs.topLeft.y, 0.625f),
            "billboard flipped UV top-left preserves source rectangle origin");
    Check(Near(uvs.topRight.x, 0.25f) && Near(uvs.topRight.y, 0.625f),
            "billboard flipped UV top-right preserves negative source width");
    Check(Near(uvs.bottomRight.x, 0.25f) && Near(uvs.bottomRight.y, 0.375f),
            "billboard flipped UV bottom-right preserves negative source height");
    Check(Near(uvs.bottomLeft.x, 0.3125f) && Near(uvs.bottomLeft.y, 0.375f),
            "billboard flipped UV bottom-left preserves negative source height");
}

void TestSectorBillboardQuadWorldPositions()
{
    const game::SectorBillboardQuad bottomCenter = game::BuildSectorBillboardQuad(
            Vector3{10.0f, 2.0f, 20.0f},
            Vector2{4.0f, 3.0f},
            Vector2{0.5f, 1.0f},
            Vector3{1.0f, 0.0f, 0.0f});

    Check(Near(bottomCenter.bottomLeft, Vector3{8.0f, 2.0f, 20.0f})
                  && Near(bottomCenter.bottomRight, Vector3{12.0f, 2.0f, 20.0f})
                  && Near(bottomCenter.topRight, Vector3{12.0f, 5.0f, 20.0f})
                  && Near(bottomCenter.topLeft, Vector3{8.0f, 5.0f, 20.0f}),
            "billboard quad bottom-center origin builds expected world corners");

    const game::SectorBillboardQuad customOrigin = game::BuildSectorBillboardQuad(
            Vector3{1.0f, 2.0f, 3.0f},
            Vector2{2.0f, 4.0f},
            Vector2{0.25f, 0.25f},
            Vector3{0.0f, 0.0f, 1.0f});

    Check(Near(customOrigin.bottomLeft, Vector3{1.0f, -1.0f, 2.5f})
                  && Near(customOrigin.bottomRight, Vector3{1.0f, -1.0f, 4.5f})
                  && Near(customOrigin.topRight, Vector3{1.0f, 3.0f, 4.5f})
                  && Near(customOrigin.topLeft, Vector3{1.0f, 3.0f, 2.5f}),
            "billboard quad custom origin and camera right build expected world corners");
}

void TestClearSectorRuntimeObjectsOnlyDestroysSectorObjects()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    state.worldReserved = true;

    const engine::Entity sectorObject = world.CreateEntity();
    world.Add(sectorObject, game::SectorObject{});
    world.Add(sectorObject, game::SectorBillboardAnimator{});

    const engine::Entity unrelatedObject = world.CreateEntity();
    world.Add(unrelatedObject, game::SectorBillboardAnimator{});

    game::ClearSectorRuntimeObjects(world, assets, state);

    Check(!world.IsAlive(sectorObject),
            "sector runtime cleanup destroys entities marked with SectorObject");
    Check(world.IsAlive(unrelatedObject),
            "sector runtime cleanup leaves unrelated ECS entities alive");
    Check(state.worldReserved,
            "sector runtime cleanup preserves world reservation bookkeeping");
    Check(state.placedObjectEntities.empty(),
            "sector runtime cleanup clears placed object entity mappings");
}

int CountSectorObjects(engine::World& world)
{
    int count = 0;
    world.ForEach<game::SectorObject>(
            [&count](engine::Entity, game::SectorObject&) {
                ++count;
            });
    return count;
}

int CountDirectionalBillboardObjects(engine::World& world)
{
    int count = 0;
    world.ForEach<game::SectorObject, game::SectorBillboardDirectionalClips>(
            [&count](engine::Entity, game::SectorObject&, game::SectorBillboardDirectionalClips&) {
                ++count;
            });
    return count;
}

int CountSingleClipBillboardObjects(engine::World& world)
{
    int count = 0;
    world.ForEach<game::SectorObject, game::SectorBillboardSingleClip>(
            [&count](engine::Entity, game::SectorObject&, game::SectorBillboardSingleClip&) {
                ++count;
            });
    return count;
}

int CountDoorObjects(engine::World& world)
{
    int count = 0;
    world.ForEach<game::SectorObject, game::SectorDoor>(
            [&count](engine::Entity, game::SectorObject&, game::SectorDoor&) {
                ++count;
            });
    return count;
}

game::SectorPlacedRuntimeObject MakePlacedBillboard(
        int id,
        Vector3 position,
        Vector2 size,
        Vector2 origin,
        bool playing,
        bool directional)
{
    game::SectorPlacedRuntimeObject object;
    object.id = id;
    object.kind = "billboard";
    object.position = position;
    object.yawRadians = 0.75f;
    object.billboard.spriteAnimationPath = "assets/sprites/goblin.json";
    object.billboard.sizeWorld = size;
    object.billboard.originNormalized = origin;
    object.billboard.playing = playing;
    object.billboard.directional = directional;
    object.billboard.clip = "Idle";
    object.billboard.frontClip = "North";
    object.billboard.backClip = "South";
    object.billboard.leftClip = "West";
    object.billboard.rightClip = "East";
    return object;
}

game::SectorPlacedRuntimeObject MakePlacedDoor(int id, game::SectorPlacedDoor door)
{
    game::SectorPlacedRuntimeObject object;
    object.id = id;
    object.kind = "door";
    object.door = std::move(door);
    return object;
}

void TestRefreshSectorRuntimeObjectMapDataReportsDoorAnchorDiagnostics()
{
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    map.runtimeObjects.push_back(MakePlacedDoor(30, MakeDoorOnPortal()));

    game::SectorPlacedDoor missingLineDoor = MakeDoorOnPortal();
    missingLineDoor.anchor.lineDefId = 999;
    map.runtimeObjects.push_back(MakePlacedDoor(31, missingLineDoor));

    game::RefreshSectorRuntimeObjectMapData(state, map);

    Check(state.doorObjectCount == 2
                  && state.validDoorAnchorCount == 1
                  && state.invalidDoorAnchorCount == 1,
            "runtime object map data counts valid and invalid door anchors");
    Check(state.doorAnchorDiagnostics.size() == 1,
            "runtime object map data stores invalid door anchor diagnostics");
    Check(state.doorAnchorDiagnostics[0].placedObjectId == 31
                  && state.doorAnchorDiagnostics[0].lineDefId == 999,
            "door anchor diagnostic stores placed object and linedef IDs");
    Check(state.doorAnchorDiagnostics[0].message.find("linedef is missing") != std::string::npos,
            "door anchor diagnostic includes resolver failure text");
}

void TestSpawnPlacedRuntimeObjectSkipsInvalidDoorAnchorWithDiagnostics()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.anchor.lineDefId = 999;
    map.runtimeObjects.push_back(MakePlacedDoor(32, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 0,
            "placed door with invalid anchor does not spawn");
    Check(state.placedObjectEntities.empty(),
            "invalid door anchor does not store placed object entity mapping");
    Check(engine::IsNull(state.runtimeObjectAssetScope),
            "invalid door anchor does not create runtime object asset scope");
    Check(state.placedObjectCount == 1 && state.spawnedObjectCount == 0 && state.skippedObjectCount == 1,
            "invalid door anchor skip records runtime object counts");
    Check(state.invalidDoorAnchorCount == 1 && state.doorAnchorDiagnostics.size() == 1,
            "invalid door anchor skip preserves door diagnostic counts");
    Check(state.placedObjectStatus.find("doors 0 valid, 1 invalid anchors") != std::string::npos,
            "invalid door anchor appears in runtime object status");
    Check(state.placedObjectWarning.find("1 door object(s) have invalid anchors") != std::string::npos,
            "invalid door anchor appears in runtime object warning");

    game::UpdateSectorRuntimeObjects(world, assets, state, map, 0.0f);
    Check(CountSectorObjects(world) == 0,
            "runtime object update does not revive invalid door data");
}

void TestSpawnPlacedDoorCopiesResolvedPayloadToEcs()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.initialOpenFraction = 0.25f;
    door.motion = game::SectorDoorMotionType::SlideRight;
    door.openDistance = 1.75f;
    door.speed = 2.5f;
    door.width = 2.0f;
    door.height = 1.5f;
    door.thickness = 0.375f;
    door.normalOffset = 0.125f;
    door.autoOpen = true;
    door.interactionDistance = 2.25f;
    door.autoOpenDistance = 3.5f;
    door.textureId = "test_door";
    map.runtimeObjects.push_back(MakePlacedDoor(35, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 1 && CountDoorObjects(world) == 1,
            "valid placed door spawns one sector door entity");
    Check(state.placedObjectEntities.size() == 1 && state.placedObjectEntities[0].placedObjectId == 35,
            "valid placed door stores placed object ID to entity mapping");
    Check(state.placedObjectCount == 1 && state.spawnedObjectCount == 1 && state.skippedObjectCount == 0,
            "valid placed door spawn records runtime object counts");
    Check(engine::IsNull(state.runtimeObjectAssetScope),
            "valid placed door spawn does not create a runtime object asset scope before material loading");
    Check(state.validDoorAnchorCount == 1 && state.invalidDoorAnchorCount == 0,
            "valid placed door preserves door anchor diagnostic counts");

    const engine::Entity entity = state.placedObjectEntities[0].entity;
    Check(world.IsAlive(entity), "valid placed door mapped entity is alive");
    Check(world.Has<game::SectorObjectTransform>(entity)
                  && world.Has<game::SectorObject>(entity)
                  && world.Has<game::SectorObjectLighting>(entity),
            "valid placed door uses shared sector object components");
    Check(world.Has<game::SectorDoor>(entity)
                  && world.Has<game::SectorDoorResolvedAnchor>(entity)
                  && world.Has<game::SectorDoorMotion>(entity)
                  && world.Has<game::SectorDoorInteraction>(entity)
                  && world.Has<game::SectorDoorRender>(entity)
                  && world.Has<game::SectorDoorCollider>(entity)
                  && world.Has<game::SectorDoorPortalBlocker>(entity),
            "valid placed door has expected door-specific components");
    Check(!world.Has<game::SectorBillboardSprite>(entity)
                  && !world.Has<game::SectorBillboardAnimator>(entity),
            "valid placed door does not add billboard components");

    const game::SectorObjectTransform& transform = world.Get<game::SectorObjectTransform>(entity);
    Check(Near(transform.position, Vector3{0.625f, 1.25f, 0.6875f}),
            "valid placed door transform uses resolved slab center, normal offset, and initial motion");
    Check(Near(transform.yawRadians, 1.57079637f),
            "valid placed door transform yaw follows portal tangent");

    const game::SectorDoor& runtimeDoor = world.Get<game::SectorDoor>(entity);
    Check(runtimeDoor.placedObjectId == 35 && runtimeDoor.enabled,
            "valid placed door stores runtime door object identity");

    const game::SectorDoorResolvedAnchor& anchor = world.Get<game::SectorDoorResolvedAnchor>(entity);
    Check(anchor.lineDefId == 2
                  && anchor.frontSectorId == 10
                  && anchor.backSectorId == 20
                  && anchor.frontSideDefId == 2
                  && anchor.backSideDefId == 8,
            "valid placed door anchor component stores resolved stable IDs");
    Check(Near(anchor.midpoint, Vector2{0.5f, 0.25f})
                  && Near(anchor.tangent, Vector2{0.0f, 1.0f})
                  && Near(anchor.normal, Vector2{1.0f, 0.0f}),
            "valid placed door anchor component stores resolved basis");
    Check(Near(anchor.openBottom, 0.5f)
                  && Near(anchor.openTop, 2.0f)
                  && Near(anchor.portalWidth, 0.5f)
                  && Near(anchor.portalHeight, 1.5f),
            "valid placed door anchor component stores resolved portal opening");

    const game::SectorDoorMotion& motion = world.Get<game::SectorDoorMotion>(entity);
    Check(motion.motion == game::SectorDoorMotionType::SlideRight
                  && Near(motion.openFraction, 0.25f)
                  && Near(motion.targetOpenFraction, 0.25f)
                  && Near(motion.openDistance, 1.75f)
                  && Near(motion.speed, 2.5f),
            "valid placed door motion component copies authored initial motion state");

    const game::SectorDoorInteraction& interaction = world.Get<game::SectorDoorInteraction>(entity);
    Check(interaction.autoOpen
                  && Near(interaction.interactionDistance, 2.25f)
                  && Near(interaction.autoOpenDistance, 3.5f),
            "valid placed door interaction component copies authored interaction settings");

    const game::SectorDoorRender& render = world.Get<game::SectorDoorRender>(entity);
    Check(Near(render.width, 2.0f)
                  && Near(render.height, 1.5f)
                  && Near(render.thickness, 0.375f)
                  && Near(render.normalOffset, 0.125f)
                  && render.textureId == "test_door"
                  && render.visible,
            "valid placed door render component stores dimensions and material ID");

    const game::SectorDoorCollider& collider = world.Get<game::SectorDoorCollider>(entity);
    Check(collider.enabled
                  && Near(collider.center, Vector2{0.625f, 0.6875f})
                  && Near(collider.tangent, Vector2{0.0f, 1.0f})
                  && Near(collider.normal, Vector2{1.0f, 0.0f})
                  && Near(collider.halfExtents, Vector2{1.0f, 0.1875f})
                  && Near(collider.bottom, 0.5f)
                  && Near(collider.top, 2.0f),
            "valid placed door collider component stores derived OBB footprint and vertical interval");

    const game::SectorDoorPortalBlocker& blocker = world.Get<game::SectorDoorPortalBlocker>(entity);
    Check(blocker.lineDefId == 2
                  && blocker.frontSectorId == 10
                  && blocker.backSectorId == 20
                  && !blocker.blocksPortal,
            "valid placed door portal blocker component stores portal identity and initial open state");
}

void TestSpawnPlacedDoorRefreshDoesNotDuplicate()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    map.runtimeObjects.push_back(MakePlacedDoor(36, MakeDoorOnPortal()));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    const engine::Entity firstEntity = state.placedObjectEntities[0].entity;

    map.runtimeObjects[0].door.initialOpenFraction = 1.0f;
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    const engine::Entity secondEntity = state.placedObjectEntities[0].entity;

    Check(CountSectorObjects(world) == 1 && CountDoorObjects(world) == 1,
            "repeated placed door spawn refresh keeps one sector door entity");
    Check(!world.IsAlive(firstEntity) && world.IsAlive(secondEntity) && firstEntity != secondEntity,
            "placed door spawn refresh replaces the mapped ECS entity");
    Check(state.placedObjectEntities.size() == 1 && state.placedObjectEntities[0].placedObjectId == 36,
            "placed door spawn refresh keeps one placed object mapping");
    Check(Near(world.Get<game::SectorDoorMotion>(secondEntity).openFraction, 1.0f)
                  && Near(world.Get<game::SectorDoorMotion>(secondEntity).targetOpenFraction, 1.0f),
            "placed door spawn refresh uses edited authored initial fraction");
}

void TestSpawnPlacedDoorDerivesDefaultOpenDistance()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor verticalDoor = MakeDoorOnPortal();
    verticalDoor.motion = game::SectorDoorMotionType::SlideVertical;
    verticalDoor.openDistance = 0.0f;
    map.runtimeObjects.push_back(MakePlacedDoor(37, verticalDoor));

    game::SectorPlacedDoor horizontalDoor = MakeDoorOnPortal();
    horizontalDoor.motion = game::SectorDoorMotionType::SlideLeft;
    horizontalDoor.openDistance = 0.0f;
    map.runtimeObjects.push_back(MakePlacedDoor(38, horizontalDoor));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(state.placedObjectEntities.size() == 2,
            "default open distance fixture spawns both doors");
    const engine::Entity verticalEntity = state.placedObjectEntities[0].entity;
    const engine::Entity horizontalEntity = state.placedObjectEntities[1].entity;
    Check(Near(world.Get<game::SectorDoorMotion>(verticalEntity).openDistance, 1.5f),
            "vertical door derives default open distance from portal height");
    Check(Near(world.Get<game::SectorDoorMotion>(horizontalEntity).openDistance, 0.5f),
            "horizontal door derives default open distance from portal width");
}

void TestSectorDoorMotionAdvancesOpenAndClosed()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);

    const engine::Entity opening = world.CreateEntity();
    world.Add(opening, game::SectorDoor{1, true});
    world.Add(opening, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.25f,
            1.0f,
            2.0f,
            1.0f});

    const engine::Entity closing = world.CreateEntity();
    world.Add(closing, game::SectorDoor{2, true});
    world.Add(closing, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideRight,
            0.75f,
            0.0f,
            4.0f,
            2.0f});

    game::AdvanceSectorDoorMotionSystem(world, 0.5f);

    Check(Near(world.Get<game::SectorDoorMotion>(opening).openFraction, 0.5f),
            "door motion opens by speed dt converted through open distance");
    Check(Near(world.Get<game::SectorDoorMotion>(closing).openFraction, 0.5f),
            "door motion closes by speed dt converted through open distance");
}

void TestSectorDoorMotionClampsAndIgnoresZeroSpeed()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 3);

    const engine::Entity clamped = world.CreateEntity();
    world.Add(clamped, game::SectorDoor{1, true});
    world.Add(clamped, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.9f,
            2.0f,
            1.0f,
            4.0f});

    const engine::Entity zeroSpeed = world.CreateEntity();
    world.Add(zeroSpeed, game::SectorDoor{2, true});
    world.Add(zeroSpeed, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideLeft,
            0.25f,
            1.0f,
            1.0f,
            0.0f});

    const engine::Entity disabled = world.CreateEntity();
    world.Add(disabled, game::SectorDoor{3, false});
    world.Add(disabled, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideRight,
            0.25f,
            1.0f,
            1.0f,
            4.0f});

    game::AdvanceSectorDoorMotionSystem(world, 0.5f);
    Check(Near(world.Get<game::SectorDoorMotion>(clamped).openFraction, 1.0f)
                  && Near(world.Get<game::SectorDoorMotion>(clamped).targetOpenFraction, 1.0f),
            "door motion clamps target and current open fraction to one");
    Check(Near(world.Get<game::SectorDoorMotion>(zeroSpeed).openFraction, 0.25f),
            "door motion with zero speed does not advance");
    Check(Near(world.Get<game::SectorDoorMotion>(disabled).openFraction, 0.25f),
            "disabled door motion does not advance");

    game::AdvanceSectorDoorMotionSystem(world, -1.0f);
    Check(Near(world.Get<game::SectorDoorMotion>(clamped).openFraction, 1.0f),
            "door motion ignores invalid negative dt");
}

void TestSectorDoorAutoOpenSetsTargetFromPlayerRange()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);

    const engine::Entity autoDoor = world.CreateEntity();
    world.Add(autoDoor, game::SectorObjectTransform{Vector3{1.0f, 0.0f, 1.0f}, 0.0f});
    world.Add(autoDoor, game::SectorDoor{1, true});
    world.Add(autoDoor, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            0.0f,
            1.0f,
            1.0f});
    world.Add(autoDoor, game::SectorDoorInteraction{true, 1.5f, 2.0f});

    const engine::Entity manualDoor = world.CreateEntity();
    world.Add(manualDoor, game::SectorObjectTransform{Vector3{1.0f, 0.0f, 1.0f}, 0.0f});
    world.Add(manualDoor, game::SectorDoor{2, true});
    world.Add(manualDoor, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            0.25f,
            1.0f,
            1.0f});
    world.Add(manualDoor, game::SectorDoorInteraction{false, 1.5f, 2.0f});

    game::UpdateSectorDoorAutoOpenSystem(world, Vector3{2.0f, 5.0f, 1.0f});

    Check(Near(world.Get<game::SectorDoorMotion>(autoDoor).targetOpenFraction, 1.0f),
            "auto-open door target opens when player is within horizontal range");
    Check(Near(world.Get<game::SectorDoorMotion>(manualDoor).targetOpenFraction, 0.25f),
            "non-auto door target is unaffected by auto-open control");

    game::UpdateSectorDoorAutoOpenSystem(world, Vector3{4.5f, 0.0f, 1.0f});

    Check(Near(world.Get<game::SectorDoorMotion>(autoDoor).targetOpenFraction, 0.0f),
            "auto-open door target closes when player leaves range");
}

void TestSectorDoorAutoOpenIgnoresDisabledAndInvalidPlayerPosition()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);

    const engine::Entity disabledDoor = world.CreateEntity();
    world.Add(disabledDoor, game::SectorObjectTransform{Vector3{1.0f, 0.0f, 1.0f}, 0.0f});
    world.Add(disabledDoor, game::SectorDoor{1, false});
    world.Add(disabledDoor, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            0.25f,
            1.0f,
            1.0f});
    world.Add(disabledDoor, game::SectorDoorInteraction{true, 1.5f, 2.0f});

    game::UpdateSectorDoorAutoOpenSystem(world, Vector3{1.0f, 0.0f, 1.0f});
    Check(Near(world.Get<game::SectorDoorMotion>(disabledDoor).targetOpenFraction, 0.25f),
            "disabled auto-open door target is unaffected");

    world.Get<game::SectorDoor>(disabledDoor).enabled = true;
    game::UpdateSectorDoorAutoOpenSystem(
            world,
            Vector3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f});
    Check(Near(world.Get<game::SectorDoorMotion>(disabledDoor).targetOpenFraction, 0.25f),
            "auto-open control ignores invalid player position");
}

void TestSectorDoorInteractTogglesNearestManualDoorInFront()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 3);

    const engine::Entity farther = world.CreateEntity();
    world.Add(farther, game::SectorObjectTransform{Vector3{1.25f, 0.0f, 0.0f}, 0.0f});
    world.Add(farther, game::SectorDoor{1, true});
    world.Add(farther, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            0.0f,
            1.0f,
            1.0f});
    world.Add(farther, game::SectorDoorInteraction{false, 2.0f, 2.0f});

    const engine::Entity nearest = world.CreateEntity();
    world.Add(nearest, game::SectorObjectTransform{Vector3{0.75f, 0.0f, 0.0f}, 0.0f});
    world.Add(nearest, game::SectorDoor{2, true});
    world.Add(nearest, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            0.0f,
            1.0f,
            1.0f});
    world.Add(nearest, game::SectorDoorInteraction{false, 2.0f, 2.0f});

    const engine::Entity autoDoor = world.CreateEntity();
    world.Add(autoDoor, game::SectorObjectTransform{Vector3{0.5f, 0.0f, 0.0f}, 0.0f});
    world.Add(autoDoor, game::SectorDoor{3, true});
    world.Add(autoDoor, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            0.0f,
            1.0f,
            1.0f});
    world.Add(autoDoor, game::SectorDoorInteraction{true, 2.0f, 2.0f});

    const bool toggled = game::ToggleTargetedSectorDoorInteractionSystem(
            world,
            Vector3{0.0f, 0.0f, 0.0f},
            Vector3{1.0f, 0.0f, 0.0f});

    Check(toggled, "manual door interaction reports a targeted door");
    Check(Near(world.Get<game::SectorDoorMotion>(nearest).targetOpenFraction, 1.0f),
            "manual door interaction opens nearest non-auto door in front");
    Check(Near(world.Get<game::SectorDoorMotion>(farther).targetOpenFraction, 0.0f),
            "manual door interaction leaves farther manual door unchanged");
    Check(Near(world.Get<game::SectorDoorMotion>(autoDoor).targetOpenFraction, 0.0f),
            "manual door interaction ignores auto-open doors");
}

void TestSectorDoorInteractRequiresFacingAndTogglesClosed()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);

    const engine::Entity door = world.CreateEntity();
    world.Add(door, game::SectorObjectTransform{Vector3{1.0f, 0.0f, 0.0f}, 0.0f});
    world.Add(door, game::SectorDoor{1, true});
    world.Add(door, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.75f,
            1.0f,
            1.0f,
            1.0f});
    world.Add(door, game::SectorDoorInteraction{false, 1.5f, 2.0f});

    const bool wrongDirection = game::ToggleTargetedSectorDoorInteractionSystem(
            world,
            Vector3{0.0f, 0.0f, 0.0f},
            Vector3{-1.0f, 0.0f, 0.0f});
    Check(!wrongDirection && Near(world.Get<game::SectorDoorMotion>(door).targetOpenFraction, 1.0f),
            "manual door interaction requires facing the door");

    const bool toggled = game::ToggleTargetedSectorDoorInteractionSystem(
            world,
            Vector3{0.0f, 0.0f, 0.0f},
            Vector3{1.0f, 0.0f, 0.0f});
    Check(toggled && Near(world.Get<game::SectorDoorMotion>(door).targetOpenFraction, 0.0f),
            "manual door interaction closes a targeted open door");
}

game::SectorDoorResolvedAnchor MakeRuntimeDoorAnchorForDerivedState()
{
    game::SectorDoorResolvedAnchor anchor;
    anchor.lineDefId = 2;
    anchor.frontSectorId = 10;
    anchor.backSectorId = 20;
    anchor.frontSideDefId = 2;
    anchor.backSideDefId = 8;
    anchor.endpointA = Vector2{0.5f, 0.0f};
    anchor.endpointB = Vector2{0.5f, 0.5f};
    anchor.midpoint = Vector2{0.5f, 0.25f};
    anchor.tangent = Vector2{0.0f, 1.0f};
    anchor.normal = Vector2{1.0f, 0.0f};
    anchor.openBottom = 0.5f;
    anchor.openTop = 2.0f;
    anchor.portalWidth = 0.5f;
    anchor.portalHeight = 1.5f;
    return anchor;
}

game::SectorDoorRender MakeDoorRenderForDerivedState()
{
    game::SectorDoorRender render;
    render.width = 2.0f;
    render.height = 1.5f;
    render.thickness = 0.25f;
    render.normalOffset = 0.125f;
    render.visible = true;
    return render;
}

engine::Entity AddDoorForDerivedState(
        engine::World& world,
        game::SectorDoorMotionType motionType,
        float openFraction,
        float openDistance)
{
    const engine::Entity entity = world.CreateEntity();
    world.Add(entity, game::SectorObjectTransform{});
    world.Add(entity, game::SectorDoor{1, true});
    world.Add(entity, MakeRuntimeDoorAnchorForDerivedState());
    world.Add(entity, game::SectorDoorMotion{
            motionType,
            openFraction,
            openFraction,
            openDistance,
            1.0f});
    world.Add(entity, MakeDoorRenderForDerivedState());
    world.Add(entity, game::SectorDoorCollider{});
    world.Add(entity, game::SectorDoorPortalBlocker{2, 10, 20, 2, 8, true});
    return entity;
}

void TestSectorDoorDerivedStateUpdatesTransformAndCollider()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 3);

    const engine::Entity closed = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    const engine::Entity verticalHalf = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.5f,
            1.5f);
    const engine::Entity rightOpen = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideRight,
            1.0f,
            0.5f);

    game::UpdateSectorDoorDerivedStateSystem(world);

    Check(Near(world.Get<game::SectorObjectTransform>(closed).position, Vector3{0.625f, 1.25f, 0.25f}),
            "closed door derived transform is centered on the portal slab");
    Check(Near(world.Get<game::SectorObjectTransform>(verticalHalf).position, Vector3{0.625f, 2.0f, 0.25f}),
            "vertical slide derived transform moves up by open fraction and distance");
    Check(Near(world.Get<game::SectorObjectTransform>(rightOpen).position, Vector3{0.625f, 1.25f, 0.75f}),
            "right slide derived transform moves along positive tangent");

    const game::SectorDoorCollider& collider = world.Get<game::SectorDoorCollider>(verticalHalf);
    Check(collider.enabled
                  && Near(collider.center, Vector2{0.625f, 0.25f})
                  && Near(collider.tangent, Vector2{0.0f, 1.0f})
                  && Near(collider.normal, Vector2{1.0f, 0.0f})
                  && Near(collider.halfExtents, Vector2{1.0f, 0.125f})
                  && Near(collider.bottom, 1.25f)
                  && Near(collider.top, 2.75f),
            "door derived collider stores current OBB footprint and vertical interval");
}

void TestSectorDoorDerivedStateUpdatesLeftSlideAndBlockerThreshold()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 3);

    const engine::Entity leftHalf = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideLeft,
            0.5f,
            0.5f);
    const engine::Entity epsilonClosed = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            game::kSectorDoorPortalBlockEpsilon,
            1.5f);
    const engine::Entity beyondEpsilon = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            game::kSectorDoorPortalBlockEpsilon + 0.0001f,
            1.5f);

    game::UpdateSectorDoorDerivedStateSystem(world);

    Check(Near(world.Get<game::SectorObjectTransform>(leftHalf).position, Vector3{0.625f, 1.25f, 0.0f}),
            "left slide derived transform moves along negative tangent");
    Check(world.Get<game::SectorDoorPortalBlocker>(epsilonClosed).blocksPortal,
            "door portal blocker remains closed at epsilon threshold");
    Check(!world.Get<game::SectorDoorPortalBlocker>(beyondEpsilon).blocksPortal,
            "door portal blocker opens beyond epsilon threshold");
}

void TestSectorDoorDynamicColliderCollectionIncludesEnabledDoorShapes()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);

    const engine::Entity closed = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    const engine::Entity fullyOpen = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            1.0f,
            1.5f);

    game::UpdateSectorDoorDerivedStateSystem(world);

    std::vector<game::SectorDynamicDoorCollider> colliders;
    colliders.reserve(2);
    game::CollectSectorDoorDynamicColliders(world, colliders);

    Check(colliders.size() == 2,
            "dynamic door collider collection includes enabled closed and fully open physical slabs");
    const game::SectorDynamicDoorCollider* closedCollider = nullptr;
    const game::SectorDynamicDoorCollider* openCollider = nullptr;
    for (const game::SectorDynamicDoorCollider& collider : colliders) {
        if (collider.entity == closed) {
            closedCollider = &collider;
        } else if (collider.entity == fullyOpen) {
            openCollider = &collider;
        }
    }

    Check(closedCollider != nullptr && closedCollider->placedObjectId == 1,
            "dynamic door collider snapshot stores placed object ID and entity for diagnostics");
    if (closedCollider != nullptr) {
        Check(Near(closedCollider->center, Vector2{0.625f, 0.25f})
                      && Near(closedCollider->tangent, Vector2{0.0f, 1.0f})
                      && Near(closedCollider->normal, Vector2{1.0f, 0.0f})
                      && Near(closedCollider->halfExtents, Vector2{1.0f, 0.125f})
                      && Near(closedCollider->bottom, 0.5f)
                      && Near(closedCollider->top, 2.0f),
                "dynamic door collider snapshot stores OBB footprint and vertical interval");
    }
    Check(openCollider != nullptr
                  && Near(openCollider->bottom, 2.0f)
                  && Near(openCollider->top, 3.5f),
            "fully open dynamic door collider snapshot uses current moved vertical interval");
}

void TestSectorDoorDynamicColliderCollectionExcludesDisabledAndInvalidShapes()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);

    const engine::Entity disabledDoor = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    const engine::Entity disabledCollider = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    const engine::Entity invalidShape = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    const engine::Entity validDoor = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideRight,
            0.5f,
            0.5f);

    game::UpdateSectorDoorDerivedStateSystem(world);
    world.Get<game::SectorDoor>(disabledDoor).enabled = false;
    world.Get<game::SectorDoorCollider>(disabledCollider).enabled = false;
    world.Get<game::SectorDoorCollider>(invalidShape).halfExtents = Vector2{};

    std::vector<game::SectorDynamicDoorCollider> colliders;
    colliders.reserve(4);
    game::CollectSectorDoorDynamicColliders(world, colliders);

    Check(colliders.size() == 1,
            "dynamic door collider collection excludes disabled doors, disabled colliders, and invalid shapes");
    Check(!colliders.empty()
                  && colliders[0].entity == validDoor
                  && Near(colliders[0].center, Vector2{0.625f, 0.5f})
                  && Near(colliders[0].bottom, 0.5f)
                  && Near(colliders[0].top, 2.0f),
            "dynamic door collider collection keeps the valid current collider snapshot");
}

game::SectorDynamicDoorCollider MakeDynamicDoorCollider(
        Vector2 center,
        Vector2 halfExtents,
        float bottom,
        float top)
{
    game::SectorDynamicDoorCollider collider;
    collider.placedObjectId = 7;
    collider.center = center;
    collider.tangent = Vector2{0.0f, 1.0f};
    collider.normal = Vector2{1.0f, 0.0f};
    collider.halfExtents = halfExtents;
    collider.bottom = bottom;
    collider.top = top;
    return collider;
}

void TestSectorDoorDynamicCollisionBlocksClosedDoor()
{
    const game::SectorCollisionMoveState moveState{
            Vector2{0.0f, 0.0f},
            0.0f,
            10,
            true};
    const game::SectorCollisionMoveResult staticResult{
            Vector2{1.0f, 0.0f},
            20,
            false,
            false,
            false};
    const std::vector<game::SectorDynamicDoorCollider> colliders{
            MakeDynamicDoorCollider(Vector2{0.75f, 0.0f}, Vector2{1.0f, 0.125f}, 0.0f, 2.0f)};

    const game::SectorCollisionMoveResult result =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    staticResult,
                    game::SectorCollisionMoveConfig{0.25f, 1.6f, 0.25f, 4},
                    colliders);

    Check(result.hitWall, "closed dynamic door reports wall contact");
    Check(result.currentSectorId == 10, "closed dynamic door preserves previous sector when crossing is blocked");
    Check(result.positionXZ.x <= 0.375f + 0.001f,
            "closed dynamic door pushes player cylinder out of slab thickness");
}

void TestSectorDoorDynamicCollisionBlocksThinDoorTunneling()
{
    const game::SectorCollisionMoveState moveState{
            Vector2{0.0f, 0.0f},
            0.0f,
            10,
            true};
    const game::SectorCollisionMoveResult staticResult{
            Vector2{1.6f, 0.0f},
            20,
            false,
            false,
            false};
    const std::vector<game::SectorDynamicDoorCollider> colliders{
            MakeDynamicDoorCollider(Vector2{1.0f, 0.0f}, Vector2{1.0f, 0.05f}, 0.0f, 2.0f)};

    const game::SectorCollisionMoveResult result =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    staticResult,
                    game::SectorCollisionMoveConfig{0.25f, 1.6f, 0.25f, 4},
                    colliders);

    Check(result.hitWall, "thin closed dynamic door reports wall contact for swept crossing");
    Check(result.currentSectorId == 10,
            "thin closed dynamic door preserves previous sector for swept crossing");
    Check(Near(result.positionXZ, moveState.positionXZ),
            "thin closed dynamic door rejects tunneled movement back to movement start");
}

void TestSectorDoorDynamicCollisionIgnoresPortalBlockerState()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);
    const engine::Entity entity = world.CreateEntity();
    world.Add(entity, game::SectorDoor{7, true});
    world.Add(entity, game::SectorDoorCollider{
            Vector2{1.0f, 0.0f},
            Vector2{0.0f, 1.0f},
            Vector2{1.0f, 0.0f},
            Vector2{1.0f, 0.05f},
            0.0f,
            2.0f,
            true});
    world.Add(entity, game::SectorDoorPortalBlocker{1, 10, 20, 2, 3, false});

    std::vector<game::SectorDynamicDoorCollider> colliders;
    game::CollectSectorDoorDynamicColliders(world, colliders);

    const game::SectorCollisionMoveState moveState{
            Vector2{0.0f, 0.0f},
            0.0f,
            10,
            true};
    const game::SectorCollisionMoveResult staticResult{
            Vector2{1.6f, 0.0f},
            20,
            false,
            false,
            false};
    const game::SectorCollisionMoveResult result =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    staticResult,
                    game::SectorCollisionMoveConfig{0.25f, 1.6f, 0.25f, 4},
                    colliders);

    Check(colliders.size() == 1,
            "dynamic door collision collection keeps enabled physical collider independent of portal blocker");
    Check(result.hitWall && Near(result.positionXZ, moveState.positionXZ),
            "dynamic door crossing collision uses physical collider state rather than portal blocker state");
}

void TestSectorDoorDynamicCollisionAllowsVerticalNonOverlap()
{
    const game::SectorCollisionMoveState moveState{
            Vector2{0.0f, 0.0f},
            0.0f,
            10,
            true};
    const game::SectorCollisionMoveResult staticResult{
            Vector2{1.0f, 0.0f},
            20,
            false,
            false,
            false};
    const std::vector<game::SectorDynamicDoorCollider> colliders{
            MakeDynamicDoorCollider(Vector2{0.75f, 0.0f}, Vector2{1.0f, 0.125f}, 2.0f, 3.5f)};

    const game::SectorCollisionMoveResult result =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    staticResult,
                    game::SectorCollisionMoveConfig{0.25f, 1.6f, 0.25f, 4},
                    colliders);

    Check(!result.hitWall, "raised dynamic door does not block when vertical intervals do not overlap");
    Check(result.currentSectorId == 20 && Near(result.positionXZ, Vector2{1.0f, 0.0f}),
            "raised dynamic door preserves static movement result");
}

void TestSectorDoorDynamicCollisionAllowsPhysicallyClearCrossing()
{
    const game::SectorCollisionMoveState moveState{
            Vector2{0.0f, 0.0f},
            0.0f,
            10,
            true};
    const game::SectorCollisionMoveResult staticResult{
            Vector2{1.6f, 0.0f},
            20,
            false,
            false,
            false};
    const std::vector<game::SectorDynamicDoorCollider> colliders{
            MakeDynamicDoorCollider(Vector2{3.0f, 0.0f}, Vector2{1.0f, 0.05f}, 0.0f, 2.0f)};

    const game::SectorCollisionMoveResult result =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    staticResult,
                    game::SectorCollisionMoveConfig{0.25f, 1.6f, 0.25f, 4},
                    colliders);

    Check(!result.hitWall, "physically clear dynamic door crossing does not block");
    Check(result.currentSectorId == 20 && Near(result.positionXZ, staticResult.positionXZ),
            "physically clear dynamic door crossing preserves static movement result");
}

void TestSectorDoorDynamicCollisionStartsInsideSafe()
{
    const game::SectorCollisionMoveState moveState{
            Vector2{0.75f, 0.0f},
            0.0f,
            10,
            true};
    const game::SectorCollisionMoveResult staticResult{
            Vector2{0.75f, 0.0f},
            10,
            false,
            false,
            false};
    const std::vector<game::SectorDynamicDoorCollider> colliders{
            MakeDynamicDoorCollider(Vector2{0.75f, 0.0f}, Vector2{1.0f, 0.125f}, 0.0f, 2.0f)};

    const game::SectorCollisionMoveResult result =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    staticResult,
                    game::SectorCollisionMoveConfig{0.25f, 1.6f, 0.25f, 4},
                    colliders);

    Check(result.hitWall, "starting inside dynamic door reports contact safely");
    Check(std::isfinite(result.positionXZ.x) && std::isfinite(result.positionXZ.y),
            "starting inside dynamic door keeps finite position");
    Check(std::fabs(result.positionXZ.x - 0.75f) >= 0.374f || std::fabs(result.positionXZ.y) >= 1.249f,
            "starting inside dynamic door resolves along a least-penetration axis");
}

void TestDoorAnchorDiagnosticsDoNotAffectValidBillboardRuntimeObject()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    map.runtimeObjects.push_back(MakePlacedBillboard(
            33,
            Vector3{16.0f, 8.0f, 16.0f},
            Vector2{1.0f, 1.0f},
            Vector2{0.5f, 1.0f},
            true,
            false));
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.anchor.lineDefId = 999;
    map.runtimeObjects.push_back(MakePlacedDoor(34, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 1,
            "door anchor diagnostics do not prevent valid billboard spawn");
    Check(state.placedObjectEntities.size() == 1 && state.placedObjectEntities[0].placedObjectId == 33,
            "door anchor diagnostics leave valid billboard entity mapping intact");
    Check(state.placedObjectCount == 2 && state.spawnedObjectCount == 1 && state.skippedObjectCount == 1,
            "mixed billboard and invalid door records expected spawn counts");
    Check(state.spriteAnimationRequestedCount == 1 && state.spriteAnimationPendingCount == 1,
            "mixed invalid door diagnostics do not change billboard asset request counts");
}

void TestSpawnPlacedRuntimeObjectSkipsLegacyGoblinDefinition()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    map.runtimeObjects.push_back(game::SectorPlacedRuntimeObject{
            1,
            "goblin",
            Vector3{1.0f, 0.5f, 1.0f},
            0.75f});

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 0,
            "legacy definitionId goblin does not spawn a hardcoded sector object entity");
    Check(state.placedObjectEntities.empty(),
            "legacy definitionId goblin does not store placed object ID to entity mapping");
    Check(engine::IsNull(state.runtimeObjectAssetScope),
            "legacy definitionId goblin does not create sector runtime object asset scope");
    Check(state.placedObjectCount == 1 && state.spawnedObjectCount == 0 && state.skippedObjectCount == 1,
            "legacy definitionId goblin skip records debug counts");
    Check(state.spriteAnimationRequestedCount == 0
                  && state.spriteAnimationPendingCount == 0
                  && state.spriteAnimationReadyCount == 0
                  && state.spriteAnimationFailedCount == 0,
            "legacy definitionId goblin skip does not request sprite animation assets");
    Check(state.directionalClipResolvedCount == 0 && state.directionalClipMissingCount == 0,
            "legacy definitionId goblin skip does not record missing directional clips");
    Check(state.placedObjectStatus.find("Runtime objects: 1 placed / 0 spawned, 1 skipped")
                  != std::string::npos,
            "legacy definitionId goblin skip records debug status");
    Check(state.placedObjectWarning.find("legacy definitionId 'goblin' for placed object 1 is unsupported")
                  != std::string::npos,
            "legacy definitionId goblin skip records debug warning");

    game::UpdateSectorRuntimeObjects(world, assets, state, map, 0.0f);
    Check(CountSectorObjects(world) == 0,
            "runtime object update does not revive legacy definitionId goblin data");
}

void TestSpawnPlacedRuntimeObjectSkipsUnsupportedKind()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorPlacedRuntimeObject unsupported;
    unsupported.id = 2;
    unsupported.kind = "unsupported";
    unsupported.position = Vector3{1.0f, 0.5f, 1.0f};
    map.runtimeObjects.push_back(unsupported);

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 0,
            "placed runtime object with unsupported kind is skipped");
    Check(state.placedObjectEntities.empty(),
            "unsupported kind does not store placed object entity mapping");
    Check(engine::IsNull(state.runtimeObjectAssetScope),
            "unsupported kind does not create runtime object asset scope");
    Check(state.placedObjectCount == 1 && state.spawnedObjectCount == 0 && state.skippedObjectCount == 1,
            "unsupported kind skip records debug counts");
    Check(state.placedObjectStatus.find("Runtime objects: 1 placed / 0 spawned, 1 skipped")
                  != std::string::npos,
            "unsupported kind skip records debug status");
    Check(state.placedObjectWarning.find("unsupported kind 'unsupported' for placed object 2")
                  != std::string::npos,
            "unsupported kind skip records debug warning");
}

void TestSpawnPlacedRuntimeObjectSkipsMissingBillboardSprite()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorPlacedRuntimeObject object = MakePlacedBillboard(
            12,
            Vector3{1.0f, 0.5f, 1.0f},
            Vector2{1.0f, 1.0f},
            Vector2{0.5f, 1.0f},
            true,
            false);
    object.billboard.spriteAnimationPath.clear();
    map.runtimeObjects.push_back(object);

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 0,
            "placed billboard with missing sprite animation path is skipped");
    Check(state.placedObjectEntities.empty(),
            "missing billboard sprite path does not store placed object entity mapping");
    Check(engine::IsNull(state.runtimeObjectAssetScope),
            "missing billboard sprite path does not create runtime object asset scope");
    Check(state.placedObjectCount == 1 && state.spawnedObjectCount == 0 && state.skippedObjectCount == 1,
            "missing billboard sprite path skip records debug counts");
    Check(state.placedObjectWarning.find("missing billboard sprite animation path for placed object 12")
                  != std::string::npos,
            "missing billboard sprite path skip records debug warning");
}

void TestSpawnPlacedBillboardCopiesAuthoredPayloadToEcs()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    map.runtimeObjects.push_back(MakePlacedBillboard(
            6,
            Vector3{16.0f, 8.0f, 32.0f},
            Vector2{2.5f, 3.5f},
            Vector2{0.25f, 0.75f},
            false,
            false));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 1,
            "generic billboard placed object spawns one sector object entity");
    Check(state.placedObjectEntities.size() == 1 && state.placedObjectEntities[0].placedObjectId == 6,
            "generic billboard stores placed object ID to entity mapping");
    Check(state.placedObjectCount == 1 && state.spawnedObjectCount == 1 && state.skippedObjectCount == 0,
            "generic billboard spawn records debug counts");
    Check(!engine::IsNull(state.runtimeObjectAssetScope),
            "generic billboard spawn creates runtime object asset scope");
    Check(state.spriteAnimationRequestedCount == 1 && state.spriteAnimationPendingCount == 1,
            "generic billboard spawn requests sprite animation asset during spawn");

    const engine::Entity entity = state.placedObjectEntities[0].entity;
    Check(world.IsAlive(entity), "generic billboard mapped entity is alive");
    const game::SectorObjectTransform& transform = world.Get<game::SectorObjectTransform>(entity);
    Check(Near(transform.position, Vector3{2.0f, 1.0f, 4.0f}),
            "generic billboard spawn converts authored position to world position");
    Check(Near(transform.yawRadians, 0.75f),
            "generic billboard spawn copies authored yaw to ECS transform");

    const game::SectorBillboardSprite& sprite = world.Get<game::SectorBillboardSprite>(entity);
    Check(!engine::IsNull(sprite.animation),
            "generic billboard spawn stores requested sprite animation handle");
    Check(Near(sprite.sizeWorld.x, 2.5f) && Near(sprite.sizeWorld.y, 3.5f),
            "generic billboard spawn copies authored world size without authoring-unit conversion");
    Check(Near(sprite.originNormalized.x, 0.25f) && Near(sprite.originNormalized.y, 0.75f),
            "generic billboard spawn copies authored normalized origin");
    Check(sprite.alphaCutoff == game::kSectorBillboardDefaultAlphaCutoff,
            "generic billboard spawn uses default alpha cutoff");

    const game::SectorBillboardAnimator& animator = world.Get<game::SectorBillboardAnimator>(entity);
    Check(animator.animationId == "assets/sprites/goblin.json",
            "generic billboard animator stores authored sprite animation path as animation id");
    Check(!animator.playing,
            "generic billboard spawn copies authored playing flag");
    Check(!world.Has<game::SectorBillboardDirectionalClips>(entity),
            "non-directional generic billboard omits directional clip component");
    Check(CountSingleClipBillboardObjects(world) == 1,
            "non-directional generic billboard adds single clip component");
    const game::SectorBillboardSingleClip& singleClip = world.Get<game::SectorBillboardSingleClip>(entity);
    Check(singleClip.name == "Idle",
            "non-directional generic billboard stores authored single clip name");
    Check(singleClip.clip == engine::InvalidSpriteClipIndex && !singleClip.resolved,
            "non-directional generic billboard single clip starts unresolved until asset is ready");
}

void TestSpawnPlacedDirectionalBillboardCopiesClipNames()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    map.runtimeObjects.push_back(MakePlacedBillboard(
            7,
            Vector3{1.0f, 0.5f, 1.0f},
            Vector2{1.0f, 2.0f},
            Vector2{0.5f, 1.0f},
            true,
            true));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 1,
            "directional generic billboard spawns one sector object entity");
    Check(CountDirectionalBillboardObjects(world) == 1,
            "directional generic billboard adds directional clip component");
    Check(CountSingleClipBillboardObjects(world) == 0,
            "directional generic billboard omits single clip component");
    const engine::Entity entity = state.placedObjectEntities[0].entity;
    const game::SectorBillboardDirectionalClips& clips =
            world.Get<game::SectorBillboardDirectionalClips>(entity);
    Check(clips.frontName == "North" && clips.backName == "South"
                  && clips.leftName == "West" && clips.rightName == "East",
            "directional generic billboard copies authored clip names");
    Check(!clips.resolved,
            "directional generic billboard clip component starts unresolved until asset is ready");
}

void TestSpawnPlacedRuntimeObjectsRefreshDoesNotDuplicate()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    map.runtimeObjects.push_back(MakePlacedBillboard(
            3,
            Vector3{16.0f, 8.0f, 16.0f},
            Vector2{1.0f, 1.0f},
            Vector2{0.5f, 1.0f},
            true,
            false));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    const engine::Entity firstEntity = state.placedObjectEntities[0].entity;
    map.runtimeObjects[0].position = Vector3{32.0f, 8.0f, 32.0f};
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    const engine::Entity secondEntity = state.placedObjectEntities[0].entity;

    Check(CountSectorObjects(world) == 1,
            "repeated generic billboard spawn refresh keeps one sector object entity");
    Check(!world.IsAlive(firstEntity) && world.IsAlive(secondEntity) && firstEntity != secondEntity,
            "generic billboard spawn refresh replaces the mapped ECS entity");
    Check(state.placedObjectEntities.size() == 1 && state.placedObjectEntities[0].placedObjectId == 3,
            "generic billboard spawn refresh keeps one placed object mapping");
    Check(Near(world.Get<game::SectorObjectTransform>(secondEntity).position, Vector3{4.0f, 1.0f, 4.0f}),
            "generic billboard spawn refresh uses edited authored position");

    game::ClearSectorRuntimeObjects(world, assets, state);
    Check(CountSectorObjects(world) == 0,
            "explicit runtime reset clears generic billboard runtime object entities");
    Check(state.placedObjectEntities.empty(),
            "explicit runtime reset clears generic billboard placed object mappings");
}

void TestPreviewRuntimeObjectRefreshKeepsAssetScope()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    map.runtimeObjects.push_back(MakePlacedBillboard(
            4,
            Vector3{16.0f, 8.0f, 16.0f},
            Vector2{1.0f, 1.0f},
            Vector2{0.5f, 1.0f},
            true,
            false));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    const engine::AssetScopeHandle firstScope = state.runtimeObjectAssetScope;
    const engine::Entity firstEntity = state.placedObjectEntities[0].entity;

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    const engine::Entity secondEntity = state.placedObjectEntities[0].entity;

    Check(CountSectorObjects(world) == 1,
            "preview runtime object refresh keeps generic billboard spawned once");
    Check(state.runtimeObjectAssetScope == firstScope,
            "preview runtime object refresh keeps existing runtime object asset scope");
    Check(!world.IsAlive(firstEntity) && world.IsAlive(secondEntity),
            "preview runtime object refresh replaces generic billboard mapped entity");
}

void TestResetSectorRuntimeObjectsForMapReloadsWithoutDuplicates()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    map.runtimeObjects.push_back(MakePlacedBillboard(
            5,
            Vector3{16.0f, 8.0f, 16.0f},
            Vector2{1.0f, 1.0f},
            Vector2{0.5f, 1.0f},
            true,
            false));

    game::ResetSectorRuntimeObjectsForMap(world, assets, state, map);
    const engine::AssetScopeHandle firstScope = state.runtimeObjectAssetScope;
    const engine::Entity firstEntity = state.placedObjectEntities[0].entity;

    map.runtimeObjects[0].position = Vector3{32.0f, 8.0f, 32.0f};
    game::ResetSectorRuntimeObjectsForMap(world, assets, state, map);
    const engine::Entity secondEntity = state.placedObjectEntities[0].entity;

    Check(CountSectorObjects(world) == 1,
            "explicit map runtime reset respawns one generic billboard");
    Check(!world.IsAlive(firstEntity),
            "explicit map runtime reset destroys previous generic billboard entity");
    Check(world.IsAlive(secondEntity) && firstScope.index != state.runtimeObjectAssetScope.index,
            "explicit map runtime reset creates a fresh runtime object asset scope");
    Check(state.placedObjectEntities.size() == 1,
            "explicit map runtime reset creates one generic billboard placed object mapping");
}

void TestSectorBillboardSpriteAnimationRequestRejectsMissingInput()
{
    engine::AssetManager assets;
    game::SectorBillboardSprite sprite;
    game::SectorBillboardAnimator animator;
    animator.animationId = "previous";
    animator.timeSeconds = 2.0f;
    animator.finished = true;
    sprite.clipIndex = 7;
    sprite.source = Rectangle{1.0f, 2.0f, 3.0f, 4.0f};
    sprite.texture = engine::TextureHandle{3, 1};

    const engine::SpriteAnimationHandle missingId = game::RequestSectorBillboardSpriteAnimation(
            assets,
            engine::NullAssetScopeHandle(),
            "",
            "assets/sprites/example.json",
            sprite,
            animator);

    Check(engine::IsNull(missingId), "billboard sprite request rejects missing animation id");
    Check(engine::IsNull(sprite.animation), "billboard sprite request leaves null animation on missing id");
    Check(sprite.clipIndex == engine::InvalidSpriteClipIndex,
            "billboard sprite request clears clip on missing id");
    Check(sprite.source.x == 0.0f && sprite.source.y == 0.0f
                  && sprite.source.width == 0.0f && sprite.source.height == 0.0f,
            "billboard sprite request clears source rectangle on missing id");
    Check(engine::IsNull(sprite.texture), "billboard sprite request clears texture on missing id");
    Check(animator.animationId.empty(), "billboard sprite request clears animation id on missing id");
    Check(animator.timeSeconds == 0.0f, "billboard sprite request resets time on missing id");
    Check(!animator.finished, "billboard sprite request clears finished flag on missing id");

    animator.animationId = "previous";
    animator.timeSeconds = 3.0f;
    animator.finished = true;
    sprite.clipIndex = 9;
    sprite.source = Rectangle{5.0f, 6.0f, 7.0f, 8.0f};
    sprite.texture = engine::TextureHandle{4, 1};

    const engine::SpriteAnimationHandle missingPath = game::RequestSectorBillboardSpriteAnimation(
            assets,
            engine::NullAssetScopeHandle(),
            "example",
            "",
            sprite,
            animator);

    Check(engine::IsNull(missingPath), "billboard sprite request rejects missing JSON path");
    Check(engine::IsNull(sprite.animation), "billboard sprite request leaves null animation on missing path");
    Check(sprite.clipIndex == engine::InvalidSpriteClipIndex,
            "billboard sprite request clears clip on missing path");
    Check(sprite.source.x == 0.0f && sprite.source.y == 0.0f
                  && sprite.source.width == 0.0f && sprite.source.height == 0.0f,
            "billboard sprite request clears source rectangle on missing path");
    Check(engine::IsNull(sprite.texture), "billboard sprite request clears texture on missing path");
    Check(animator.animationId.empty(), "billboard sprite request clears animation id on missing path");
    Check(animator.timeSeconds == 0.0f, "billboard sprite request resets time on missing path");
    Check(!animator.finished, "billboard sprite request clears finished flag on missing path");
}

void TestBillboardSpriteFixtureIsOrdinaryAssetData()
{
    const char* goblinJsonPath = ASSETS_PATH "sprites/goblin.json";
    FILE* goblinJson = std::fopen(goblinJsonPath, "rb");
    Check(goblinJson != nullptr, "checked-in billboard sprite fixture exists at assets/sprites/goblin.json");
    if (goblinJson != nullptr) {
        std::fclose(goblinJson);
    }

    Json goblinDocument;
    Check(LoadJsonFile(goblinJsonPath, goblinDocument),
            "checked-in billboard sprite fixture JSON loads");
    if (!goblinDocument.is_null()) {
        Check(goblinDocument.contains("meta")
                      && goblinDocument["meta"].contains("image")
                      && goblinDocument["meta"]["image"].get<std::string>() == "goblin.png",
                "checked-in billboard sprite fixture references relative atlas image");
        const char* goblinPngPath = ASSETS_PATH "sprites/goblin.png";
        FILE* goblinPng = std::fopen(goblinPngPath, "rb");
        Check(goblinPng != nullptr,
                "checked-in billboard sprite fixture atlas exists at assets/sprites/goblin.png");
        if (goblinPng != nullptr) {
            std::fclose(goblinPng);
        }
    }
}

void TestPlacedBillboardStoresDirectionalClipNamesAsStrings()
{
    const game::SectorPlacedRuntimeObject object = MakePlacedBillboard(
            4,
            Vector3{1.0f, 2.0f, 3.0f},
            Vector2{0.8f, 1.2f},
            Vector2{0.5f, 1.0f},
            true,
            true);

    Check(object.kind == "billboard" && object.definitionId.empty(),
            "placed billboard stores generic kind without legacy definition id");
    Check(object.billboard.spriteAnimationPath == "assets/sprites/goblin.json",
            "placed billboard stores sprite fixture as authored sprite path");
    Check(object.billboard.frontClip == std::string("North"),
            "placed billboard stores front clip name as authored string");
    Check(object.billboard.backClip == std::string("South"),
            "placed billboard stores back clip name as authored string");
    Check(object.billboard.leftClip == std::string("West"),
            "placed billboard stores left clip name as authored string");
    Check(object.billboard.rightClip == std::string("East"),
            "placed billboard stores right clip name as authored string");
}

void TestSectorBillboardAnimatorAdvances()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);

    const engine::Entity object = world.CreateEntity();
    game::SectorBillboardAnimator animator;
    animator.timeSeconds = 1.0f;
    animator.speed = 2.0f;
    world.Add(object, animator);

    game::AdvanceSectorBillboardAnimatorSystem(world, 0.25f);
    const game::SectorBillboardAnimator& advanced = world.Get<game::SectorBillboardAnimator>(object);
    Check(advanced.timeSeconds == 1.5f,
            "billboard animator advances by dt times speed");

    game::AdvanceSectorBillboardAnimatorSystem(world, -1.0f);
    Check(world.Get<game::SectorBillboardAnimator>(object).timeSeconds == 1.5f,
            "billboard animator ignores invalid negative dt");
}

void TestSectorBillboardAnimatorDoesNotAdvanceWhenPaused()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);

    const engine::Entity pausedObject = world.CreateEntity();
    game::SectorBillboardAnimator paused;
    paused.timeSeconds = 1.0f;
    paused.playing = false;
    world.Add(pausedObject, paused);

    const engine::Entity playingObject = world.CreateEntity();
    game::SectorBillboardAnimator playing;
    playing.timeSeconds = 1.0f;
    playing.playing = true;
    world.Add(playingObject, playing);

    game::AdvanceSectorBillboardAnimatorSystem(world, 0.5f);

    Check(world.Get<game::SectorBillboardAnimator>(pausedObject).timeSeconds == 1.0f,
            "paused billboard animator does not advance time");
    Check(world.Get<game::SectorBillboardAnimator>(playingObject).timeSeconds == 1.5f,
            "playing billboard animator advances time");
}

engine::SpriteAnimationAsset MakeDirectionalClipAsset(bool includeLeft, bool includeDefault)
{
    engine::SpriteAnimationAsset asset;
    asset.frames.resize(5);
    asset.clips.push_back(engine::SpriteClip{"Front", 0, 1, engine::SpritePlaybackMode::Loop, 0});
    asset.clips.push_back(engine::SpriteClip{"Back", 1, 1, engine::SpritePlaybackMode::Loop, 0});
    if (includeLeft) {
        asset.clips.push_back(engine::SpriteClip{"Left", 2, 1, engine::SpritePlaybackMode::Loop, 0});
    }
    asset.clips.push_back(engine::SpriteClip{"Right", 3, 1, engine::SpritePlaybackMode::Loop, 0});
    if (includeDefault) {
        asset.clips.push_back(engine::SpriteClip{"Default", 4, 1, engine::SpritePlaybackMode::Loop, 0});
    }
    return asset;
}

engine::SpriteAnimationAsset MakeSingleClipAsset(bool includeIdle, bool includeDefault)
{
    engine::SpriteAnimationAsset asset;
    asset.frames.resize(3);
    if (includeIdle) {
        asset.clips.push_back(engine::SpriteClip{"Idle", 0, 1, engine::SpritePlaybackMode::Loop, 0});
    }
    asset.clips.push_back(engine::SpriteClip{"Walk", 1, 1, engine::SpritePlaybackMode::Loop, 0});
    if (includeDefault) {
        asset.clips.push_back(engine::SpriteClip{"Default", 2, 1, engine::SpritePlaybackMode::Loop, 0});
    }
    return asset;
}

void TestSectorBillboardSingleClipResolve()
{
    const engine::SpriteAnimationAsset asset = MakeSingleClipAsset(true, false);

    game::SectorBillboardSingleClip clip;
    const bool resolved = game::ResolveSectorBillboardSingleClipFromAsset(asset, "Idle", clip);

    Check(resolved, "billboard single clip resolver succeeds when named clip exists");
    Check(clip.resolved && !clip.usedFallback,
            "billboard single clip resolver marks exact mapping resolved without fallback");
    Check(clip.name == "Idle" && clip.clip == 0,
            "billboard single clip resolver stores resolved clip index");
}

void TestSectorBillboardSingleClipEmptyFallsBackToFirstClip()
{
    const engine::SpriteAnimationAsset asset = MakeSingleClipAsset(true, false);

    game::SectorBillboardSingleClip clip;
    const bool resolved = game::ResolveSectorBillboardSingleClipFromAsset(asset, "", clip);

    Check(resolved, "billboard single clip resolver succeeds for empty/default clip");
    Check(clip.resolved && !clip.usedFallback,
            "billboard single clip resolver treats empty/default clip as normal resolution");
    Check(clip.name.empty() && clip.clip == 0,
            "billboard single clip resolver uses first clip when no Default clip exists");
}

void TestSectorBillboardSingleClipMissingFallsBackToDefaultClip()
{
    const engine::SpriteAnimationAsset asset = MakeSingleClipAsset(false, true);

    game::SectorBillboardSingleClip clip;
    const bool resolved = game::ResolveSectorBillboardSingleClipFromAsset(asset, "Idle", clip);

    Check(resolved, "billboard single clip resolver succeeds with Default fallback");
    Check(clip.resolved && clip.usedFallback,
            "billboard single clip resolver records fallback for missing clip");
    Check(clip.name == "Idle" && clip.clip == 1,
            "billboard single clip resolver uses Default clip for missing named clip");
}

void TestSectorBillboardSingleClipMissingWithoutFallback()
{
    engine::SpriteAnimationAsset asset;

    game::SectorBillboardSingleClip clip;
    clip.clip = 4;
    clip.resolved = true;
    clip.usedFallback = true;
    const bool resolved = game::ResolveSectorBillboardSingleClipFromAsset(asset, "Idle", clip);

    Check(!resolved, "billboard single clip resolver reports missing clip when asset has no clips");
    Check(!clip.resolved && !clip.usedFallback && clip.clip == engine::InvalidSpriteClipIndex,
            "billboard single clip resolver clears state when no fallback is available");
    Check(clip.name == "Idle",
            "billboard single clip resolver preserves requested missing clip name");
}

void TestSectorBillboardDirectionalClipsResolve()
{
    const char* goblinJsonPath = ASSETS_PATH "sprites/goblin.json";
    FILE* goblinJson = std::fopen(goblinJsonPath, "rb");
    Check(goblinJson != nullptr, "test goblin Aseprite JSON exists at assets/sprites/goblin.json");
    if (goblinJson != nullptr) {
        std::fclose(goblinJson);
    }

    const engine::SpriteAnimationAsset asset = MakeDirectionalClipAsset(true, false);

    game::SectorBillboardDirectionalClips clips;
    const bool resolved = game::ResolveSectorBillboardDirectionalClipsFromAsset(
            asset,
            game::SectorBillboardDirectionalClipNames{},
            clips);

    Check(resolved, "billboard directional clip resolver succeeds when all named clips exist");
    Check(clips.resolved, "billboard directional clip resolver marks mapping resolved");
    Check(!clips.usedFallback, "billboard directional clip resolver does not use fallback for complete clips");
    Check(clips.front == 0, "billboard directional clip resolver stores Front clip index");
    Check(clips.back == 1, "billboard directional clip resolver stores Back clip index");
    Check(clips.left == 2, "billboard directional clip resolver stores Left clip index");
    Check(clips.right == 3, "billboard directional clip resolver stores Right clip index");
}

void TestSectorBillboardDirectionalClipsFallback()
{
    const engine::SpriteAnimationAsset asset = MakeDirectionalClipAsset(false, true);

    game::SectorBillboardDirectionalClips clips;
    const bool resolved = game::ResolveSectorBillboardDirectionalClipsFromAsset(
            asset,
            game::SectorBillboardDirectionalClipNames{},
            clips);

    Check(resolved, "billboard directional clip resolver succeeds with fallback clip");
    Check(clips.resolved, "billboard directional clip fallback marks mapping resolved");
    Check(clips.usedFallback, "billboard directional clip fallback records fallback use");
    Check(clips.front == 0, "billboard directional clip fallback keeps Front clip index");
    Check(clips.back == 1, "billboard directional clip fallback keeps Back clip index");
    Check(clips.left == 3, "billboard directional clip fallback uses Default clip index for missing Left");
    Check(clips.right == 2, "billboard directional clip fallback keeps Right clip index");
}

void TestSectorBillboardDirectionalClipsMissingWithoutFallback()
{
    const engine::SpriteAnimationAsset asset = MakeDirectionalClipAsset(false, false);

    game::SectorBillboardDirectionalClips clips;
    const bool resolved = game::ResolveSectorBillboardDirectionalClipsFromAsset(
            asset,
            game::SectorBillboardDirectionalClipNames{},
            clips);

    Check(!resolved, "billboard directional clip resolver reports missing clips without fallback");
    Check(!clips.resolved && !clips.usedFallback,
            "billboard directional clip resolver leaves unresolved state when required clip is missing");
    Check(clips.left == engine::InvalidSpriteClipIndex,
            "billboard directional clip resolver leaves missing direction invalid without fallback");
}

void TestSectorBillboardDirectionalClipsRejectUnavailableAsset()
{
    engine::AssetManager assets;
    game::SectorBillboardDirectionalClips clips;
    clips.front = 5;
    clips.resolved = true;
    clips.usedFallback = true;

    const bool resolved = game::ResolveSectorBillboardDirectionalClips(
            assets,
            engine::NullSpriteAnimationHandle(),
            game::SectorBillboardDirectionalClipNames{},
            clips);

    Check(!resolved, "billboard directional clip resolver rejects missing animation handle");
    Check(clips.front == engine::InvalidSpriteClipIndex
                  && clips.back == engine::InvalidSpriteClipIndex
                  && clips.left == engine::InvalidSpriteClipIndex
                  && clips.right == engine::InvalidSpriteClipIndex,
            "billboard directional clip resolver clears indices when asset is unavailable");
    Check(!clips.resolved && !clips.usedFallback,
            "billboard directional clip resolver clears state when asset is unavailable");
    Check(clips.frontName == "Front"
                  && clips.backName == "Back"
                  && clips.leftName == "Left"
                  && clips.rightName == "Right",
            "billboard directional clip resolver preserves stored names when asset is unavailable");
}

game::SectorBillboardDirectionalClips MakeResolvedDirectionalClips()
{
    game::SectorBillboardDirectionalClips clips;
    clips.front = 10;
    clips.back = 11;
    clips.left = 12;
    clips.right = 13;
    clips.resolved = true;
    return clips;
}

void TestSectorBillboardDirectionalClipSelection()
{
    const game::SectorObjectTransform transform{Vector3{0.0f, 0.0f, 0.0f}, 0.0f};
    const game::SectorBillboardDirectionalClips clips = MakeResolvedDirectionalClips();

    Check(game::SelectSectorBillboardDirectionalClip(transform, Vector3{4.0f, 0.0f, 0.0f}, clips) == clips.front,
            "billboard direction selection maps camera in front of yaw-zero object to Front");
    Check(game::SelectSectorBillboardDirectionalClip(transform, Vector3{-4.0f, 0.0f, 0.0f}, clips) == clips.back,
            "billboard direction selection maps camera behind yaw-zero object to Back");
    Check(game::SelectSectorBillboardDirectionalClip(transform, Vector3{0.0f, 0.0f, -4.0f}, clips) == clips.left,
            "billboard direction selection maps camera on object left side to Left");
    Check(game::SelectSectorBillboardDirectionalClip(transform, Vector3{0.0f, 0.0f, 4.0f}, clips) == clips.right,
            "billboard direction selection maps camera on object right side to Right");
}

void TestSectorBillboardDirectionalClipSelectionWraparound()
{
    constexpr float Pi = 3.14159265358979323846f;
    const game::SectorBillboardDirectionalClips clips = MakeResolvedDirectionalClips();
    const game::SectorObjectTransform nearPositivePi{Vector3{0.0f, 0.0f, 0.0f}, Pi - 0.05f};
    const game::SectorObjectTransform nearNegativePi{Vector3{0.0f, 0.0f, 0.0f}, -Pi + 0.05f};

    Check(game::SelectSectorBillboardDirectionalClip(
                  nearPositivePi,
                  Vector3{-4.0f, 0.0f, -0.1f},
                  clips) == clips.front,
            "billboard direction selection wraps near positive pi for front-facing camera");
    Check(game::SelectSectorBillboardDirectionalClip(
                  nearNegativePi,
                  Vector3{-4.0f, 0.0f, 0.1f},
                  clips) == clips.front,
            "billboard direction selection wraps near negative pi for front-facing camera");
}

void TestSectorRuntimeObjectCurrentSectorSystem()
{
    const game::SectorTopologyMap map = MakeSquareMap();
    game::SectorCollisionWorld collisionWorld;
    std::string error;
    Check(collisionWorld.BuildFromTopology(map, &error), "runtime object sector lookup collision world builds");

    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);
    const engine::Entity object = world.CreateEntity();
    world.Add(object, game::SectorObjectTransform{Vector3{0.25f, 0.0f, 0.25f}, 0.0f});
    world.Add(object, game::SectorObject{});

    game::UpdateSectorObjectCurrentSectorSystem(world, collisionWorld);

    const game::SectorObject& sectorObject = world.Get<game::SectorObject>(object);
    Check(sectorObject.currentSectorId == 10,
            "runtime object current sector system writes containing sector id");

    game::SectorObjectTransform& transform = world.Get<game::SectorObjectTransform>(object);
    transform.position = Vector3{2.0f, 0.0f, 2.0f};

    game::UpdateSectorObjectCurrentSectorSystem(world, collisionWorld);
    Check(sectorObject.currentSectorId == -1,
            "runtime object current sector system uses negative sector id outside topology");
}

void TestSectorRuntimeObjectBakedLightingSystem()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);
    const engine::Entity object = world.CreateEntity();
    world.Add(object, game::SectorObjectTransform{Vector3{2.0f, 1.0f, 2.0f}, 0.0f});
    world.Add(object, game::SectorObject{10, true});
    world.Add(object, game::SectorObjectLighting{});

    game::UpdateSectorObjectBakedLightingSystem(world, MakeProbeRuntimeData(), nullptr);

    const game::SectorObjectLighting& lighting = world.Get<game::SectorObjectLighting>(object);
    Check(lighting.baked.valid,
            "runtime object baked lighting system stores valid probe sample");
    Check(Near(lighting.baked.ambientCube[0], Vector3{0.8f, 0.25f, 0.1f}),
            "runtime object baked lighting system stores sampled ambient cube");
}

void TestSectorRuntimeObjectBakedLightingFallback()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);
    const engine::Entity object = world.CreateEntity();
    world.Add(object, game::SectorObjectTransform{Vector3{2.0f, 1.0f, 2.0f}, 0.0f});
    world.Add(object, game::SectorObject{-1, true});
    world.Add(object, game::SectorObjectLighting{});

    const game::SectorBakedObjectLightProbeRuntimeData missingProbes;
    game::UpdateSectorObjectBakedLightingSystem(world, missingProbes, nullptr);

    const game::SectorObjectLighting& lighting = world.Get<game::SectorObjectLighting>(object);
    Check(!lighting.baked.valid,
            "runtime object baked lighting fallback is marked invalid without loaded probes");
    Check(Near(lighting.baked.ambientCube[0], Vector3{0.15f, 0.15f, 0.15f}),
            "runtime object baked lighting fallback stores neutral ambient cube");
}

void TestSectorRuntimeObjectBakedLightingUsesMapFallback()
{
    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorTopologySector* sector = game::FindSectorTopologySector(map, 10);
    Check(sector != nullptr, "runtime object baked lighting map fallback fixture has sector");
    if (sector != nullptr) {
        sector->ambientColor = Color{64, 128, 255, 255};
        sector->ambientIntensity = 0.5f;
    }

    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);
    const engine::Entity object = world.CreateEntity();
    world.Add(object, game::SectorObjectTransform{Vector3{0.25f, 1.0f, 0.25f}, 0.0f});
    world.Add(object, game::SectorObject{10, true});
    world.Add(object, game::SectorObjectLighting{});

    const game::SectorBakedObjectLightProbeRuntimeData missingProbes;
    game::UpdateSectorObjectBakedLightingSystem(world, missingProbes, &map);

    const game::SectorObjectLighting& lighting = world.Get<game::SectorObjectLighting>(object);
    Check(!lighting.baked.valid,
            "runtime object baked lighting map fallback is marked fallback when probes are unavailable");
    Check(Near(lighting.baked.ambientCube[0], Vector3{0.125490f, 0.250980f, 0.5f}),
            "runtime object baked lighting system uses sector ambient fallback when map is supplied");
}

} // namespace

int main()
{
    TestResolveSectorDoorAnchorValidPortal();
    TestResolveSectorDoorAnchorRejectsOneSidedWall();
    TestResolveSectorDoorAnchorRejectsSectorMismatch();
    TestResolveSectorDoorAnchorRejectsZeroHeightOpening();
    TestResolveSectorDoorAnchorUsesAuthoredDimensionsWhenPresent();
    TestSectorRuntimeObjectComponentsIterateAndDestroy();
    TestSectorBillboardFrameUvsUseSourceRectangle();
    TestSectorBillboardFrameUvsPreserveFlippedSourceSigns();
    TestSectorBillboardQuadWorldPositions();
    TestClearSectorRuntimeObjectsOnlyDestroysSectorObjects();
    TestRefreshSectorRuntimeObjectMapDataReportsDoorAnchorDiagnostics();
    TestSpawnPlacedRuntimeObjectSkipsInvalidDoorAnchorWithDiagnostics();
    TestSpawnPlacedDoorCopiesResolvedPayloadToEcs();
    TestSpawnPlacedDoorRefreshDoesNotDuplicate();
    TestSpawnPlacedDoorDerivesDefaultOpenDistance();
    TestSectorDoorMotionAdvancesOpenAndClosed();
    TestSectorDoorMotionClampsAndIgnoresZeroSpeed();
    TestSectorDoorAutoOpenSetsTargetFromPlayerRange();
    TestSectorDoorAutoOpenIgnoresDisabledAndInvalidPlayerPosition();
    TestSectorDoorInteractTogglesNearestManualDoorInFront();
    TestSectorDoorInteractRequiresFacingAndTogglesClosed();
    TestSectorDoorDerivedStateUpdatesTransformAndCollider();
    TestSectorDoorDerivedStateUpdatesLeftSlideAndBlockerThreshold();
    TestSectorDoorDynamicColliderCollectionIncludesEnabledDoorShapes();
    TestSectorDoorDynamicColliderCollectionExcludesDisabledAndInvalidShapes();
    TestSectorDoorDynamicCollisionBlocksClosedDoor();
    TestSectorDoorDynamicCollisionBlocksThinDoorTunneling();
    TestSectorDoorDynamicCollisionIgnoresPortalBlockerState();
    TestSectorDoorDynamicCollisionAllowsVerticalNonOverlap();
    TestSectorDoorDynamicCollisionAllowsPhysicallyClearCrossing();
    TestSectorDoorDynamicCollisionStartsInsideSafe();
    TestDoorAnchorDiagnosticsDoNotAffectValidBillboardRuntimeObject();
    TestSpawnPlacedRuntimeObjectSkipsLegacyGoblinDefinition();
    TestSpawnPlacedRuntimeObjectSkipsUnsupportedKind();
    TestSpawnPlacedRuntimeObjectSkipsMissingBillboardSprite();
    TestSpawnPlacedBillboardCopiesAuthoredPayloadToEcs();
    TestSpawnPlacedDirectionalBillboardCopiesClipNames();
    TestSpawnPlacedRuntimeObjectsRefreshDoesNotDuplicate();
    TestPreviewRuntimeObjectRefreshKeepsAssetScope();
    TestResetSectorRuntimeObjectsForMapReloadsWithoutDuplicates();
    TestSectorBillboardSpriteAnimationRequestRejectsMissingInput();
    TestBillboardSpriteFixtureIsOrdinaryAssetData();
    TestPlacedBillboardStoresDirectionalClipNamesAsStrings();
    TestSectorBillboardAnimatorAdvances();
    TestSectorBillboardAnimatorDoesNotAdvanceWhenPaused();
    TestSectorBillboardSingleClipResolve();
    TestSectorBillboardSingleClipEmptyFallsBackToFirstClip();
    TestSectorBillboardSingleClipMissingFallsBackToDefaultClip();
    TestSectorBillboardSingleClipMissingWithoutFallback();
    TestSectorBillboardDirectionalClipsResolve();
    TestSectorBillboardDirectionalClipsFallback();
    TestSectorBillboardDirectionalClipsMissingWithoutFallback();
    TestSectorBillboardDirectionalClipsRejectUnavailableAsset();
    TestSectorBillboardDirectionalClipSelection();
    TestSectorBillboardDirectionalClipSelectionWraparound();
    TestSectorRuntimeObjectCurrentSectorSystem();
    TestSectorRuntimeObjectBakedLightingSystem();
    TestSectorRuntimeObjectBakedLightingFallback();
    TestSectorRuntimeObjectBakedLightingUsesMapFallback();

    if (failures != 0) {
        std::fprintf(stderr, "%d sector runtime object test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
