#include "sector_demo/SectorRuntimeObjects.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcNavigationSystem.h"
#include "game/npc/NpcRuntime.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorMath.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"
#include "util/json.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <raymath.h>

namespace {

using Json = nlohmann::ordered_json;

int failures = 0;
constexpr float kExpectedDoorParkingEpsilonWorld = 0.01f;

float EffectiveDoorOpenDistance(float openDistance)
{
    return openDistance > kExpectedDoorParkingEpsilonWorld
            ? openDistance - kExpectedDoorParkingEpsilonWorld
            : openDistance;
}

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

bool NearTranslation(Matrix actual, Vector3 expected, float epsilon = 0.00001f)
{
    return Near(actual.m12, expected.x, epsilon)
            && Near(actual.m13, expected.y, epsilon)
            && Near(actual.m14, expected.z, epsilon);
}

size_t DoorMeshFaceIndex(game::SectorDoorFace face)
{
    switch (face) {
        case game::SectorDoorFace::Front:
            return 0;
        case game::SectorDoorFace::Back:
            return 1;
        case game::SectorDoorFace::Right:
            return 2;
        case game::SectorDoorFace::Left:
            return 3;
        case game::SectorDoorFace::Top:
            return 4;
        case game::SectorDoorFace::Bottom:
            return 5;
        case game::SectorDoorFace::Count:
            break;
    }
    return 0;
}

Vector2 DoorMeshFaceUvSpan(const game::SectorDoorSlabMeshData& mesh, game::SectorDoorFace face)
{
    const size_t base = DoorMeshFaceIndex(face) * 4;
    if (mesh.vertices.size() < base + 4) {
        return Vector2{};
    }

    float minU = mesh.vertices[base].uv.x;
    float maxU = mesh.vertices[base].uv.x;
    float minV = mesh.vertices[base].uv.y;
    float maxV = mesh.vertices[base].uv.y;
    for (size_t i = 1; i < 4; ++i) {
        const Vector2 uv = mesh.vertices[base + i].uv;
        if (uv.x < minU) {
            minU = uv.x;
        }
        if (uv.x > maxU) {
            maxU = uv.x;
        }
        if (uv.y < minV) {
            minV = uv.y;
        }
        if (uv.y > maxV) {
            maxV = uv.y;
        }
    }
    return Vector2{maxU - minU, maxV - minV};
}

bool DoorMeshFaceUvsMatch(
        const game::SectorDoorSlabMeshData& actual,
        const game::SectorDoorSlabMeshData& expected,
        game::SectorDoorFace face)
{
    const size_t base = DoorMeshFaceIndex(face) * 4;
    if (actual.vertices.size() < base + 4 || expected.vertices.size() < base + 4) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        if (!Near(actual.vertices[base + i].uv, expected.vertices[base + i].uv)) {
            return false;
        }
    }
    return true;
}

bool SameColor(Color actual, Color expected)
{
    return actual.r == expected.r
            && actual.g == expected.g
            && actual.b == expected.b
            && actual.a == expected.a;
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

game::SectorTopologyMap MakeNavigationSquareMap()
{
    game::SectorTopologyMap map;
    map.sectors.push_back(Sector(10));
    AddSectorLoop(map, 10, {{0, 0}, {2048, 0}, {2048, 2048}, {0, 2048}});
    return map;
}

game::SectorTopologyMap MakeNavigationStairMap()
{
    game::SectorTopologyMap map;
    constexpr int StepCount = 6;
    constexpr game::SectorCoord TreadDepth = 128;
    constexpr game::SectorCoord StairWidth = 512;
    for (int boundary = 0; boundary <= StepCount; ++boundary) {
        const game::SectorCoord x = boundary * TreadDepth;
        map.vertices.push_back({boundary * 2 + 1, x, 0});
        map.vertices.push_back({boundary * 2 + 2, x, StairWidth});
    }
    int nextLineId = 1;
    int nextSideId = 1;
    const auto addLine = [&](int startVertexId, int endVertexId,
                             int frontSectorId, int backSectorId = 0) {
        const int lineId = nextLineId++;
        const int frontSideId = nextSideId++;
        const int backSideId = backSectorId != 0 ? nextSideId++ : -1;
        map.lineDefs.push_back({lineId, startVertexId, endVertexId,
                frontSideId, backSideId});
        AddSide(map, frontSideId, lineId,
                game::SectorTopologySideKind::Front, frontSectorId);
        if (backSectorId != 0) {
            AddSide(map, backSideId, lineId,
                    game::SectorTopologySideKind::Back, backSectorId);
        }
    };
    for (int step = 0; step < StepCount; ++step) {
        const int sectorId = step + 1;
        game::SectorTopologySector sector = Sector(sectorId);
        sector.floorZ = static_cast<float>(step) * 1.6f;
        sector.ceilingZ = sector.floorZ + 24.0f;
        map.sectors.push_back(sector);
        const int leftBottom = step * 2 + 1;
        const int leftTop = step * 2 + 2;
        const int rightBottom = (step + 1) * 2 + 1;
        const int rightTop = (step + 1) * 2 + 2;
        addLine(leftBottom, rightBottom, sectorId);
        addLine(rightTop, leftTop, sectorId);
        if (step == 0) addLine(leftTop, leftBottom, sectorId);
        if (step == StepCount - 1) {
            addLine(rightBottom, rightTop, sectorId);
        } else {
            addLine(rightBottom, rightTop, sectorId, sectorId + 1);
        }
    }
    return map;
}

void FinishNavigationBuild(
        game::SectorNavigationWorld& navigation,
        const game::SectorTopologyMap& map,
        const std::vector<game::SectorStaticModelCollider>& colliders = {})
{
    for (int iteration = 0; iteration < 1000
            && (navigation.State() == game::SectorNavigationState::Queued
                || navigation.State() == game::SectorNavigationState::Building);
            ++iteration) {
        navigation.UpdateBuild(map, colliders, 0);
    }
}

engine::Entity SpawnNavigationTestNpc(
        engine::World& world,
        const char* instanceId,
        int placedObjectId,
        Vector3 position,
        int sectorId,
        float walkSpeed = 2.0f,
        float runSpeed = 4.0f)
{
    const engine::Entity entity = world.CreateEntity();
    game::SectorObjectTransform transform;
    transform.position = position;
    transform.yawRadians = 0.35f;
    world.Add(entity, transform);
    game::SectorObject object;
    object.currentSectorId = sectorId;
    world.Add(entity, object);
    world.Add(entity, game::SectorObjectLighting{});
    world.Add(entity, game::SectorObjectVisualOffset{});
    game::SectorDynamicModel dynamicModel;
    dynamicModel.placedObjectId = placedObjectId;
    world.Add(entity, dynamicModel);
    game::NpcRuntimeInstance npc;
    npc.definitionId = "navigation_test";
    npc.instanceId = instanceId;
    npc.walkSpeed = walkSpeed;
    npc.runSpeed = runSpeed;
    world.Add(entity, npc);
    world.Add(entity, game::NpcAnimationState{});
    return entity;
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

game::SectorTopologyMap MakeReversedDoorPortalMap()
{
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorTopologyLineDef* portal = game::FindSectorTopologyLineDef(map, 2);
    if (portal != nullptr) {
        portal->startVertexId = 3;
        portal->endVertexId = 2;
    }
    game::FindSectorTopologySideDef(map, 2)->sectorId = 20;
    game::FindSectorTopologySideDef(map, 8)->sectorId = 10;
    return map;
}

game::SectorPlacedDoor MakeDoorOnReversedPortal()
{
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.anchor.frontSectorId = 20;
    door.anchor.backSectorId = 10;
    door.anchor.endpointAX = 64;
    door.anchor.endpointAY = 64;
    door.anchor.endpointBX = 64;
    door.anchor.endpointBY = 0;
    return door;
}

game::SectorTopologyMap MakeHorizontalDoorPortalMap()
{
    game::SectorTopologyMap map;
    map.vertices = {
            {1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64},
            {5, 64, 128}, {6, 0, 128}
    };
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, -1},
            {3, 3, 4, 3, 8},
            {4, 4, 1, 4, -1},
            {5, 3, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 4, 7, -1}
    };
    AddSide(map, 1, 1, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 3, game::SectorTopologySideKind::Back, 20);

    game::SectorTopologySector front = Sector(10);
    front.floorZ = 0.0f;
    front.ceilingZ = 16.0f;
    map.sectors.push_back(front);

    game::SectorTopologySector back = Sector(20);
    back.floorZ = 0.0f;
    back.ceilingZ = 16.0f;
    map.sectors.push_back(back);
    return map;
}

game::SectorPlacedDoor MakeDoorOnHorizontalPortal()
{
    game::SectorPlacedDoor door;
    door.anchor.lineDefId = 3;
    door.anchor.frontSectorId = 10;
    door.anchor.backSectorId = 20;
    door.anchor.frontSideDefId = 3;
    door.anchor.backSideDefId = 8;
    door.anchor.endpointAX = 64;
    door.anchor.endpointAY = 64;
    door.anchor.endpointBX = 0;
    door.anchor.endpointBY = 64;
    return door;
}

game::SectorBakedObjectLightProbeRuntimeData MakeProbeRuntimeData()
{
    game::SectorBakedObjectLightProbeRuntimeData probes;

    game::SectorBakedObjectLightProbe lower;
    lower.sectorId = 10;
    lower.layer = game::SectorBakedObjectLightProbeLayer::Lower;
    lower.position = Vector3{2.0f, 0.6f, 2.0f};
    for (Vector3& face : lower.ambientCube) {
        face = Vector3{0.8f, 0.25f, 0.1f};
    }
    probes.probes.push_back(lower);

    game::SectorBakedObjectLightProbe upper;
    upper.sectorId = 10;
    upper.layer = game::SectorBakedObjectLightProbeLayer::Upper;
    upper.position = Vector3{2.0f, 1.5f, 2.0f};
    for (Vector3& face : upper.ambientCube) {
        face = Vector3{0.2f, 0.35f, 0.9f};
    }
    probes.probes.push_back(upper);

    probes.sectorRanges.push_back(game::SectorBakedObjectLightProbeSectorRange{
            10, 0, 1, game::SectorBakedObjectLightProbeLayer::Lower});
    probes.sectorRanges.push_back(game::SectorBakedObjectLightProbeSectorRange{
            10, 1, 1, game::SectorBakedObjectLightProbeLayer::Upper});

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

void TestResolveSectorDoorAnchorToleratesStaleEndpointDiagnostics()
{
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.anchor.endpointAX = -999;
    door.anchor.endpointAY = -999;
    door.anchor.endpointBX = -888;
    door.anchor.endpointBY = -888;

    game::FindSectorTopologyVertex(map, 2)->x = 80;
    game::FindSectorTopologyVertex(map, 3)->x = 80;

    const game::SectorResolvedDoorAnchor resolved = game::ResolveSectorDoorAnchor(map, door);

    Check(resolved.valid, "stale door endpoint diagnostics do not invalidate a stable portal anchor");
    Check(resolved.diagnostic.empty(), "stale door endpoint diagnostics do not report a resolver warning");
    Check(Near(resolved.endpointA, game::SectorCoordToWorldPosition2(80, 0))
                  && Near(resolved.endpointB, game::SectorCoordToWorldPosition2(80, 64)),
          "resolved door anchor uses current linedef vertices instead of stale endpoint diagnostics");
}

void TestResolveSectorDoorAnchorReversedEndpointOrderPreservesFrontBackNormal()
{
    const game::SectorTopologyMap map = MakeReversedDoorPortalMap();
    const game::SectorPlacedDoor door = MakeDoorOnReversedPortal();

    const game::SectorResolvedDoorAnchor resolved = game::ResolveSectorDoorAnchor(map, door);

    Check(resolved.valid, "reversed endpoint door portal resolves");
    Check(resolved.frontSectorId == 20 && resolved.backSectorId == 10,
          "reversed endpoint door anchor preserves stored front/back sectors");
    Check(Near(resolved.endpointA, game::SectorCoordToWorldPosition2(64, 64))
                  && Near(resolved.endpointB, game::SectorCoordToWorldPosition2(64, 0)),
          "reversed endpoint door anchor uses current linedef order");
    Check(Near(resolved.tangent, Vector2{0.0f, -1.0f})
                  && Near(resolved.normal, Vector2{-1.0f, 0.0f}),
          "reversed endpoint door anchor normal still points front sector to back sector");
}

void TestResolveSectorDoorAnchorHorizontalPortalBasis()
{
    const game::SectorTopologyMap map = MakeHorizontalDoorPortalMap();
    const game::SectorPlacedDoor door = MakeDoorOnHorizontalPortal();

    const game::SectorResolvedDoorAnchor resolved = game::ResolveSectorDoorAnchor(map, door);

    Check(resolved.valid, "horizontal two-sided door portal resolves");
    Check(resolved.frontSectorId == 10 && resolved.backSectorId == 20,
          "horizontal door anchor preserves stored front/back sectors");
    Check(Near(resolved.tangent, Vector2{-1.0f, 0.0f})
                  && Near(resolved.normal, Vector2{0.0f, 1.0f}),
          "horizontal door anchor resolves deterministic tangent and front-to-back normal");
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

engine::Entity FindPlacedObjectEntity(
        const game::SectorRuntimeObjectState& state,
        int placedObjectId)
{
    for (const game::SectorPlacedRuntimeObjectEntity& entry :
            state.placedObjectEntities) {
        if (entry.placedObjectId == placedObjectId) {
            return entry.entity;
        }
    }
    return engine::NullEntity();
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

void TestSpawnPlacedDoorPositiveNormalOffsetMovesTowardBackSector()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.width = 2.0f;
    door.height = 1.5f;
    door.thickness = 0.25f;
    door.normalOffset = 0.125f;
    map.runtimeObjects.push_back(MakePlacedDoor(33, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountDoorObjects(world) == 1, "normal offset fixture spawns one door entity");
    const engine::Entity entity = state.placedObjectEntities[0].entity;
    const game::SectorDoorResolvedAnchor& anchor = world.Get<game::SectorDoorResolvedAnchor>(entity);
    const game::SectorObjectTransform& transform = world.Get<game::SectorObjectTransform>(entity);
    const game::SectorDoorRender& render = world.Get<game::SectorDoorRender>(entity);
    const game::SectorDoorCollider& collider = world.Get<game::SectorDoorCollider>(entity);
    const Vector2 delta{
            transform.position.x - anchor.midpoint.x,
            transform.position.z - anchor.midpoint.y};
    const float signedOffset = delta.x * anchor.normal.x + delta.y * anchor.normal.y;

    Check(anchor.frontSectorId == 10
                  && anchor.backSectorId == 20
                  && Near(anchor.normal, Vector2{1.0f, 0.0f})
                  && Near(signedOffset, render.normalOffset),
          "positive normalOffset moves closed door center from front sector toward back sector");
    Check(Near(collider.center, Vector2{transform.position.x, transform.position.z})
                  && Near(collider.normal, anchor.normal),
          "door collider consumes the same resolved normal as the transform");

    const game::SectorDoorSlabGeometry slab =
            game::BuildSectorDoorSlabGeometry(transform, anchor, render);
    Check(Near(slab.normal, Vector3{anchor.normal.x, 0.0f, anchor.normal.y}),
          "door slab geometry consumes the same resolved normal as the collider");

    std::vector<game::SectorDynamicDoorCollider> colliders;
    game::CollectSectorDoorDynamicColliders(world, colliders);
    Check(colliders.size() == 1
                  && Near(colliders[0].center, collider.center)
                  && Near(colliders[0].normal, anchor.normal),
          "dynamic door collider snapshot consumes the same resolved normal");
}

void TestSpawnPlacedDoorHeightOffsetMovesSlabAndColliderWithoutResizing()
{
    for (float heightOffset : {0.375f, -0.5f}) {
        engine::World world;
        engine::AssetManager assets;
        game::SectorRuntimeObjectState state;
        game::SectorTopologyMap map = MakeDoorPortalMap();
        game::SectorPlacedDoor door = MakeDoorOnPortal();
        door.width = 2.0f;
        door.height = 1.5f;
        door.thickness = 0.25f;
        door.openDistance = 2.25f;
        door.heightOffsetWorld = heightOffset;
        map.runtimeObjects.push_back(MakePlacedDoor(34, door));

        game::RefreshSectorRuntimeObjectMapData(state, map);
        game::SpawnPlacedRuntimeObjects(world, assets, state, map);

        Check(CountDoorObjects(world) == 1,
              "height offset fixture spawns one procedural door entity");
        const engine::Entity entity = state.placedObjectEntities[0].entity;
        const game::SectorDoorResolvedAnchor& anchor =
                world.Get<game::SectorDoorResolvedAnchor>(entity);
        const game::SectorObjectTransform& transform =
                world.Get<game::SectorObjectTransform>(entity);
        const game::SectorDoorRender& render =
                world.Get<game::SectorDoorRender>(entity);
        const game::SectorDoorMotion& motion =
                world.Get<game::SectorDoorMotion>(entity);
        const game::SectorDoorCollider& collider =
                world.Get<game::SectorDoorCollider>(entity);
        const float expectedBottom = anchor.openBottom + heightOffset;

        Check(Near(render.heightOffsetWorld, heightOffset)
                      && Near(render.width, door.width)
                      && Near(render.height, door.height)
                      && Near(render.thickness, door.thickness)
                      && Near(motion.travelAmount, door.openDistance),
              "door height offset preserves procedural dimensions and authored travel");
        Check(Near(transform.position.y, expectedBottom + door.height * 0.5f)
                      && Near(collider.bottom, expectedBottom)
                      && Near(collider.top, expectedBottom + door.height),
              "door height offset moves the procedural slab transform and collider together");
    }
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
    door.openSoundId = "door_open";
    door.closeSoundId = "door_close";
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
    const float expectedMotionOffset = game::SmootherStep01(door.initialOpenFraction)
            * EffectiveDoorOpenDistance(door.openDistance);
    Check(world.IsAlive(entity), "valid placed door mapped entity is alive");
    Check(world.Has<game::SectorObjectTransform>(entity)
                  && world.Has<game::SectorObject>(entity)
                  && world.Has<game::SectorObjectLighting>(entity),
            "valid placed door uses shared sector object components");
    Check(world.Has<game::SectorDoor>(entity)
                  && world.Has<game::SectorDoorResolvedAnchor>(entity)
                  && world.Has<game::SectorDoorMotion>(entity)
                  && world.Has<game::SectorDoorAudio>(entity)
                  && world.Has<game::SectorDoorInteraction>(entity)
                  && world.Has<game::SectorDoorRender>(entity)
                  && world.Has<game::SectorDoorCollider>(entity)
                  && world.Has<game::SectorDoorPortalBlocker>(entity),
            "valid placed door has expected door-specific components");
    Check(!world.Has<game::SectorBillboardSprite>(entity)
                  && !world.Has<game::SectorBillboardAnimator>(entity),
            "valid placed door does not add billboard components");

    const game::SectorObjectTransform& transform = world.Get<game::SectorObjectTransform>(entity);
    Check(Near(transform.position, Vector3{0.625f, 1.25f, 0.25f + expectedMotionOffset}),
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
                  && Near(motion.travelAmount, 1.75f)
                  && Near(motion.travelSpeed, 2.5f),
            "valid placed door motion component copies authored initial motion state");

    const game::SectorDoorInteraction& interaction = world.Get<game::SectorDoorInteraction>(entity);
    Check(interaction.autoOpen
                  && Near(interaction.interactionDistance, 2.25f)
                  && Near(interaction.autoOpenDistance, 3.5f),
            "valid placed door interaction component copies authored interaction settings");

    const game::SectorDoorAudio& audio = world.Get<game::SectorDoorAudio>(entity);
    Check(audio.openSoundId == "door_open"
                  && audio.closeSoundId == "door_close"
                  && engine::IsNull(audio.openSound)
                  && engine::IsNull(audio.closeSound)
                  && !audio.targetWasOpen
                  && audio.pendingEvent == game::SectorDoorAudioEvent::None,
            "valid placed door audio component copies IDs without loading during spawn");

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
                  && Near(collider.center, Vector2{0.625f, 0.25f + expectedMotionOffset})
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

void TestUnknownModelSwingDoorUsesAnimatedProceduralFallback()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.visual = game::SectorDoorVisualType::Model;
    door.modelAssetId = "missing_style";
    door.motion = game::SectorDoorMotionType::Swing;
    door.initialOpenFraction = 0.75f;
    map.runtimeObjects.push_back(MakePlacedDoor(37, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(state.swingDoorCatalogLoaded && state.swingDoorCatalog.assets.size() == 20,
          "runtime map refresh retains the generated CPU swing door catalog");
    Check(state.doorFallbackCount == 1 && state.doorFallbackDiagnostics.size() == 1,
          "unknown model style records one stable fallback diagnostic");
    Check(state.doorFallbackDiagnostics[0].placedObjectId == 37
                  && state.doorFallbackDiagnostics[0].modelAssetId == "missing_style"
                  && state.doorFallbackDiagnostics[0].message.find("missing from the swing door catalog")
                          != std::string::npos,
          "unknown model style diagnostic identifies object and catalog ID");
    Check(CountDoorObjects(world) == 1 && state.spawnedObjectCount == 1,
          "unknown model style still spawns a procedural door entity");
    Check(engine::IsNull(state.runtimeObjectAssetScope),
          "unknown model style does not create an asset scope");

    const engine::Entity entity = state.placedObjectEntities[0].entity;
    const game::SectorDoorMotion& motion = world.Get<game::SectorDoorMotion>(entity);
    const game::SectorDoorPortalBlocker& blocker =
            world.Get<game::SectorDoorPortalBlocker>(entity);
    const game::SectorDoorRender& render = world.Get<game::SectorDoorRender>(entity);
    Check(motion.motion == game::SectorDoorMotionType::Swing
                  && Near(motion.openFraction, 0.75f)
                  && Near(motion.targetOpenFraction, 0.75f)
                  && Near(motion.travelAmount, 90.0f * DEG2RAD)
                  && Near(motion.travelSpeed, 90.0f * DEG2RAD)
                  && !blocker.blocksPortal,
          "unknown model swing fallback honors authored motion and portal state");
    Check(Near(render.width, 0.5f)
                  && Near(render.height, 1.5f)
                  && Near(render.thickness, 0.25f),
          "unknown model fallback uses resolved target slab dimensions");

    Check(world.Has<game::SectorDoorModelRender>(entity)
                  && world.Get<game::SectorDoorModelRender>(entity).fallbackReason
                          == game::SectorDoorModelFallbackReason::MissingCatalogAsset,
          "unknown style retains explicit model fallback state on the swing entity");

    game::UpdateSectorRuntimeObjects(world, assets, state, map, 1.0f);
    Check(Near(world.Get<game::SectorDoorMotion>(entity).openFraction, 0.75f),
          "steady runtime update leaves a swing fallback at its stable target");
}

void TestKnownModelSwingDoorUsesUniformCatalogFallbackDimensions()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.visual = game::SectorDoorVisualType::Model;
    door.modelAssetId = "wooden_interior_001";
    door.modelFit = game::SectorDoorModelFit::FitInside;
    door.modelScale = 1.0f;
    door.motion = game::SectorDoorMotionType::Swing;
    door.heightOffsetWorld = 0.4f;
    map.runtimeObjects.push_back(MakePlacedDoor(38, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SectorSwingDoorCatalogAsset catalogAsset;
    Check(game::FindSectorSwingDoorCatalogAsset(
                  state.swingDoorCatalog, door.modelAssetId, catalogAsset),
          "known runtime model style resolves from retained catalog");
    const game::SectorResolvedDoorAnchor resolved =
            game::ResolveSectorDoorAnchor(map, door);
    const game::SectorSwingDoorFitResult expectedFit =
            game::ComputeSectorSwingDoorFit(
                    catalogAsset,
                    resolved.width,
                    resolved.height,
                    door.modelFit,
                    door.modelScale);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    const engine::Entity entity = state.placedObjectEntities[0].entity;
    const game::SectorDoorRender& render = world.Get<game::SectorDoorRender>(entity);
    Check(expectedFit.status == game::SectorSwingDoorFitStatus::Valid
                  && Near(render.width, expectedFit.actualWidth)
                  && Near(render.height, expectedFit.actualHeight)
                  && Near(render.thickness, expectedFit.actualThickness),
          "known model fallback slab uses one catalog-derived uniform scale");
    Check(state.doorFallbackDiagnostics.empty()
                  && state.doorFallbackCount == 0
                  && world.Has<game::SectorDoorModelRender>(entity)
                  && !engine::IsNull(world.Get<game::SectorDoorModelRender>(entity).leafModel)
                  && world.Get<game::SectorDoorModelRender>(entity).frameDeclared
                  && !engine::IsNull(world.Get<game::SectorDoorModelRender>(entity).frameModel)
                  && Near(world.Get<game::SectorDoorModelRender>(entity).effectiveScale,
                          expectedFit.effectiveScale),
          "known model style requests its leaf and optional frame and stores its uniform fit");
    const game::SectorDoorResolvedAnchor& runtimeAnchor =
            world.Get<game::SectorDoorResolvedAnchor>(entity);
    const game::SectorDoorModelRender& model =
            world.Get<game::SectorDoorModelRender>(entity);
    const Vector3 expectedHinge{
            runtimeAnchor.midpoint.x
                    - runtimeAnchor.tangent.x
                            * catalogAsset.leafHingeToFrameCenter
                            * expectedFit.effectiveScale,
            runtimeAnchor.openBottom
                    + door.heightOffsetWorld
                    + catalogAsset.leafBottomOffset * expectedFit.effectiveScale,
            runtimeAnchor.midpoint.y
                    - runtimeAnchor.tangent.y
                            * catalogAsset.leafHingeToFrameCenter
                            * expectedFit.effectiveScale};
    const Vector3 expectedFrameOrigin{
            runtimeAnchor.midpoint.x,
            runtimeAnchor.openBottom + door.heightOffsetWorld,
            runtimeAnchor.midpoint.y};
    Check(Near(Vector3Transform(Vector3{}, model.leafMatrix), expectedHinge)
                  && Near(Vector3Transform(Vector3{}, model.frameMatrix), expectedFrameOrigin),
          "spawned framed swing preserves paired leaf inset/bottom alignment around its fixed frame");
    Check(render.alignLeafToFrame
                  && Near(render.heightOffsetWorld, door.heightOffsetWorld)
                  && Near(render.leafHingeToFrameCenter,
                          catalogAsset.leafHingeToFrameCenter
                                  * expectedFit.effectiveScale)
                  && Near(render.leafBottomOffset,
                          catalogAsset.leafBottomOffset * expectedFit.effectiveScale)
                  && Near(expectedFit.assemblyWidth,
                          catalogAsset.frameOuterWidth * expectedFit.effectiveScale)
                  && Near(expectedFit.assemblyHeight,
                          catalogAsset.frameOuterHeight * expectedFit.effectiveScale),
          "runtime render retains scaled assembly alignment and framed fit dimensions");
    std::vector<game::SectorReceiverBounds> receiverBounds;
    game::CollectSectorDoorReceiverBounds(world, receiverBounds);
    Check(receiverBounds.size() == 2
                  && Near(receiverBounds[0].min.y,
                          runtimeAnchor.openBottom + door.heightOffsetWorld)
                  && receiverBounds[0].max.y
                          >= runtimeAnchor.openBottom
                                  + door.heightOffsetWorld + render.height,
          "spawned model swing contributes analytic leaf/frame receiver fallback bounds to both adjacent sectors");
    const game::SectorDoorCollider& collider =
            world.Get<game::SectorDoorCollider>(entity);
    Check(Near(collider.bottom, expectedHinge.y)
                  && Near(collider.top, expectedHinge.y + render.height),
          "framed swing height offset moves its physical leaf collider with the model assembly");
}

void TestRetainedCatalogFailureDoesNotBlockOtherDoors()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor slideDoor = MakeDoorOnPortal();
    slideDoor.initialOpenFraction = 0.25f;
    map.runtimeObjects.push_back(MakePlacedDoor(39, slideDoor));
    game::SectorPlacedDoor modelDoor = MakeDoorOnPortal();
    modelDoor.visual = game::SectorDoorVisualType::Model;
    modelDoor.modelAssetId = "wooden_interior_001";
    modelDoor.motion = game::SectorDoorMotionType::Swing;
    map.runtimeObjects.push_back(MakePlacedDoor(40, modelDoor));
    game::SectorPlacedRuntimeObject unrelatedObject;
    unrelatedObject.id = 41;
    unrelatedObject.kind = "static_model";
    map.runtimeObjects.push_back(unrelatedObject);

    game::RefreshSectorRuntimeObjectMapData(state, map);
    state.swingDoorCatalogLoaded = false;
    state.swingDoorCatalog = game::SectorSwingDoorCatalog{};
    state.swingDoorCatalogWarning =
            "Runtime object warnings: swing door catalog unavailable: test failure";
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountDoorObjects(world) == 2
                  && CountSectorObjects(world) == 3
                  && state.spawnedObjectCount == 3
                  && state.skippedObjectCount == 0,
          "catalog failure does not prevent procedural doors or unrelated runtime objects spawning");
    Check(state.doorFallbackCount == 1
                  && state.doorFallbackDiagnostics[0].message.find("catalog failed")
                          != std::string::npos,
          "entity-only respawn refreshes fallback diagnostics from retained catalog state");
    Check(state.placedObjectWarning == state.swingDoorCatalogWarning,
          "catalog failure surfaces one stable runtime warning");

    const engine::Entity slideEntity = state.placedObjectEntities[0].entity;
    const engine::Entity modelEntity = state.placedObjectEntities[1].entity;
    Check(Near(world.Get<game::SectorDoorMotion>(slideEntity).openFraction, 0.25f)
                  && Near(world.Get<game::SectorDoorMotion>(modelEntity).openFraction, 0.0f),
          "catalog failure preserves existing slide state while model fallback stays closed");
}

void TestNullSwingDoorLeafRequestKeepsRuntimeFallback()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.visual = game::SectorDoorVisualType::Model;
    door.modelAssetId = "wooden_interior_001";
    door.motion = game::SectorDoorMotionType::Swing;
    door.initialOpenFraction = 0.5f;
    map.runtimeObjects.push_back(MakePlacedDoor(42, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    const size_t assetIndex = state.swingDoorCatalog.assetIndexById[door.modelAssetId];
    state.swingDoorCatalog.assets[assetIndex].leafModelPath.clear();
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    const engine::Entity entity = state.placedObjectEntities[0].entity;
    const game::SectorDoorModelRender& model =
            world.Get<game::SectorDoorModelRender>(entity);
    Check(CountDoorObjects(world) == 1
                  && engine::IsNull(model.leafModel)
                  && model.fallbackReason
                          == game::SectorDoorModelFallbackReason::LeafRequestFailed
                  && state.doorFallbackCount == 1
                  && state.doorFallbackDiagnostics.size() == 1,
          "null model handle keeps the swing entity and records one stable procedural fallback diagnostic");
    Check(Near(world.Get<game::SectorDoorMotion>(entity).openFraction, 0.5f)
                  && !world.Get<game::SectorDoorPortalBlocker>(entity).blocksPortal,
          "null model handle does not disable swing kinematics or force the portal closed");
}

void TestProceduralSwingDoorRunsWithoutCatalogStyle()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.motion = game::SectorDoorMotionType::Swing;
    door.initialOpenFraction = 1.0f;
    map.runtimeObjects.push_back(MakePlacedDoor(41, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    const engine::Entity entity = state.placedObjectEntities[0].entity;
    Check(state.doorFallbackCount == 0
                  && state.doorFallbackDiagnostics.empty()
                  && Near(world.Get<game::SectorDoorMotion>(entity).openFraction, 1.0f)
                  && !world.Get<game::SectorDoorPortalBlocker>(entity).blocksPortal
                  && world.Has<game::SectorDoorModelRender>(entity)
                  && !world.Get<game::SectorDoorModelRender>(entity).modelVisualRequested,
          "procedural swing works without requiring a catalog style");
}

void TestMixedDoorRuntimeRefreshRecoveryAndScopeLifecycle()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();

    const std::array<game::SectorDoorMotionType, 3> slideMotions{
            game::SectorDoorMotionType::SlideVertical,
            game::SectorDoorMotionType::SlideLeft,
            game::SectorDoorMotionType::SlideRight};
    for (size_t i = 0; i < slideMotions.size(); ++i) {
        game::SectorPlacedDoor slide = MakeDoorOnPortal();
        slide.motion = slideMotions[i];
        slide.initialOpenFraction = 0.2f * static_cast<float>(i + 1);
        slide.autoOpen = true;
        map.runtimeObjects.push_back(MakePlacedDoor(
                50 + static_cast<int>(i), slide));
    }

    game::SectorPlacedDoor modelDoor = MakeDoorOnPortal();
    modelDoor.visual = game::SectorDoorVisualType::Model;
    modelDoor.modelAssetId = "wooden_interior_001";
    modelDoor.motion = game::SectorDoorMotionType::Swing;
    modelDoor.initialOpenFraction = 0.4f;
    map.runtimeObjects.push_back(MakePlacedDoor(53, modelDoor));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountDoorObjects(world) == 4
                  && state.spawnedObjectCount == 4
                  && state.skippedObjectCount == 0
                  && state.doorFallbackCount == 0,
          "mixed procedural-slider and model-swing map spawns every door without fallback");
    for (size_t i = 0; i < slideMotions.size(); ++i) {
        const engine::Entity entity = FindPlacedObjectEntity(
                state, 50 + static_cast<int>(i));
        Check(world.IsAlive(entity)
                      && world.Get<game::SectorDoorMotion>(entity).motion
                              == slideMotions[i]
                      && Near(
                              world.Get<game::SectorDoorMotion>(entity)
                                      .openFraction,
                              0.2f * static_cast<float>(i + 1))
                      && !world.Has<game::SectorDoorModelRender>(entity),
              "mixed map preserves each procedural slide motion and initial fraction");
    }

    engine::Entity modelEntity = FindPlacedObjectEntity(state, 53);
    Check(world.IsAlive(modelEntity)
                  && world.Has<game::SectorDoorModelRender>(modelEntity)
                  && Near(world.Get<game::SectorDoorMotion>(modelEntity).openFraction, 0.4f)
                  && !world.Get<game::SectorDoorPortalBlocker>(modelEntity).blocksPortal,
          "mixed map preserves the model swing door partial-open state");
    game::SectorDoorModelRender& pendingModel =
            world.Get<game::SectorDoorModelRender>(modelEntity);
    const engine::ModelHandle firstLeaf = pendingModel.leafModel;
    const engine::ModelHandle firstFrame = pendingModel.frameModel;
    const engine::AssetScopeHandle firstScope = state.runtimeObjectAssetScope;
    Check(!engine::IsNull(firstScope)
                  && !engine::IsNull(firstLeaf)
                  && !engine::IsNull(firstFrame)
                  && game::ResolveSectorDoorModelDrawPolicy(
                             pendingModel, false, false).drawProcedural,
          "pending model requests retain the procedural slab in the shared runtime scope");

    const BoundingBox analyticBounds = pendingModel.receiverBounds;
    pendingModel.leafReady = true;
    pendingModel.leafBoundsReady = true;
    pendingModel.leafLocalBounds = BoundingBox{
            Vector3{0.0f, 0.0f, -0.25f},
            Vector3{4.0f, 3.0f, 0.25f}};
    game::UpdateSectorDoorDerivedStateSystem(world);
    const game::SectorDoorModelDrawPolicy readyPolicy =
            game::ResolveSectorDoorModelDrawPolicy(pendingModel, true, false);
    Check(readyPolicy.drawLeaf
                  && !readyPolicy.drawProcedural
                  && (pendingModel.receiverBounds.max.x > analyticBounds.max.x
                          || pendingModel.receiverBounds.max.y > analyticBounds.max.y
                          || pendingModel.receiverBounds.max.z > analyticBounds.max.z),
          "CPU readiness transition replaces fallback and expands analytic receiver bounds");

    game::SectorDoorInteraction& interaction =
            world.Get<game::SectorDoorInteraction>(modelEntity);
    interaction.autoOpen = true;
    const game::SectorDoorResolvedAnchor& anchor =
            world.Get<game::SectorDoorResolvedAnchor>(modelEntity);
    game::UpdateSectorDoorAutoOpenSystem(
            world,
            Vector3{anchor.midpoint.x, anchor.openBottom, anchor.midpoint.y});
    Check(Near(world.Get<game::SectorDoorMotion>(modelEntity).targetOpenFraction, 1.0f),
          "model swing door participates in portal-based auto-open");
    interaction.autoOpen = false;
    const bool interacted = game::ToggleTargetedSectorDoorInteractionSystem(
            world,
            Vector3{anchor.midpoint.x - 1.0f, anchor.openBottom, anchor.midpoint.y},
            Vector3{1.0f, 0.0f, 0.0f});
    Check(interacted
                  && Near(world.Get<game::SectorDoorMotion>(modelEntity).targetOpenFraction, 0.0f),
          "model swing door participates in manual portal-based interaction");

    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    const engine::Entity refreshedModelEntity = FindPlacedObjectEntity(state, 53);
    const game::SectorDoorModelRender& refreshedModel =
            world.Get<game::SectorDoorModelRender>(refreshedModelEntity);
    Check(CountDoorObjects(world) == 4
                  && refreshedModelEntity != modelEntity
                  && !world.IsAlive(modelEntity)
                  && state.runtimeObjectAssetScope == firstScope
                  && refreshedModel.leafModel == firstLeaf
                  && refreshedModel.frameModel == firstFrame
                  && state.doorFallbackDiagnostics.empty(),
          "entity-only refresh replaces mixed door entities while reusing scope and model requests");

    map.runtimeObjects.back().door.modelAssetId = "missing_style";
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    Check(state.doorFallbackCount == 1
                  && state.doorFallbackDiagnostics.size() == 1
                  && world.Get<game::SectorDoorModelRender>(
                             FindPlacedObjectEntity(state, 53)).fallbackReason
                          == game::SectorDoorModelFallbackReason::MissingCatalogAsset,
          "edited invalid style produces one stable procedural fallback diagnostic");
    map.runtimeObjects.back().door.modelAssetId = "wooden_interior_001";
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    Check(state.doorFallbackCount == 0
                  && state.doorFallbackDiagnostics.empty()
                  && world.Get<game::SectorDoorModelRender>(
                             FindPlacedObjectEntity(state, 53)).catalogResolved,
          "restoring a valid style clears diagnostics and recovers model requests");

    const engine::Entity beforeClear = FindPlacedObjectEntity(state, 53);
    game::ClearSectorRuntimeObjects(world, assets, state);
    Check(CountDoorObjects(world) == 0
                  && !world.IsAlive(beforeClear)
                  && engine::IsNull(state.runtimeObjectAssetScope)
                  && assets.IsFinished(firstLeaf)
                  && assets.IsFinished(firstFrame),
          "runtime shutdown destroys mixed doors and retires model handles with their scope");

    game::ResetSectorRuntimeObjectsForMap(world, assets, state, map);
    const engine::Entity rebuiltModelEntity = FindPlacedObjectEntity(state, 53);
    const game::SectorDoorModelRender& rebuiltModel =
            world.Get<game::SectorDoorModelRender>(rebuiltModelEntity);
    Check(CountDoorObjects(world) == 4
                  && world.IsAlive(rebuiltModelEntity)
                  && state.runtimeObjectAssetScope != firstScope
                  && rebuiltModel.leafModel != firstLeaf
                  && rebuiltModel.frameModel != firstFrame,
          "scene rebuild creates fresh mixed-door entities, scope, and model handles");
    game::ClearSectorRuntimeObjects(world, assets, state);
}

void TestSectorDoorSlabGeometryIsFiniteAndStable()
{
    const auto SpawnDoorWithNormalOffset = [](float normalOffset,
                                               engine::World& world,
                                               engine::AssetManager& assets,
                                               game::SectorRuntimeObjectState& state) {
        game::SectorTopologyMap map = MakeDoorPortalMap();
        game::SectorPlacedDoor door = MakeDoorOnPortal();
        door.width = 2.0f;
        door.height = 1.5f;
        door.thickness = 0.25f;
        door.normalOffset = normalOffset;
        map.runtimeObjects.push_back(MakePlacedDoor(39, door));

        game::RefreshSectorRuntimeObjectMapData(state, map);
        game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    };

    for (float normalOffset : {0.0f, 0.25f}) {
        engine::World world;
        engine::AssetManager assets;
        game::SectorRuntimeObjectState state;
        SpawnDoorWithNormalOffset(normalOffset, world, assets, state);
        Check(CountDoorObjects(world) == 1, "slab geometry fixture spawns one door entity");

        const engine::Entity entity = state.placedObjectEntities[0].entity;
        const game::SectorDoorSlabGeometry first = game::BuildSectorDoorSlabGeometry(
                world.Get<game::SectorObjectTransform>(entity),
                world.Get<game::SectorDoorResolvedAnchor>(entity),
                world.Get<game::SectorDoorRender>(entity));
        const game::SectorDoorSlabGeometry second = game::BuildSectorDoorSlabGeometry(
                world.Get<game::SectorObjectTransform>(entity),
                world.Get<game::SectorDoorResolvedAnchor>(entity),
                world.Get<game::SectorDoorRender>(entity));

        Check(game::IsFiniteVector3(first.tangent)
                      && game::IsFiniteVector3(first.normal)
                      && game::IsFiniteVector3(first.bottomFrontLeft)
                      && game::IsFiniteVector3(first.bottomFrontRight)
                      && game::IsFiniteVector3(first.bottomBackRight)
                      && game::IsFiniteVector3(first.bottomBackLeft)
                      && game::IsFiniteVector3(first.topFrontLeft)
                      && game::IsFiniteVector3(first.topFrontRight)
                      && game::IsFiniteVector3(first.topBackRight)
                      && game::IsFiniteVector3(first.topBackLeft),
                "door slab geometry produces finite basis and corners");
        Check(Near(first.tangent, second.tangent)
                      && Near(first.normal, second.normal)
                      && Near(first.bottomFrontLeft, second.bottomFrontLeft)
                      && Near(first.bottomFrontRight, second.bottomFrontRight)
                      && Near(first.bottomBackRight, second.bottomBackRight)
                      && Near(first.bottomBackLeft, second.bottomBackLeft)
                      && Near(first.topFrontLeft, second.topFrontLeft)
                      && Near(first.topFrontRight, second.topFrontRight)
                      && Near(first.topBackRight, second.topBackRight)
                      && Near(first.topBackLeft, second.topBackLeft),
                "door slab geometry is stable for unchanged door state");
        Check(Near(Vector3Distance(first.bottomFrontLeft, first.bottomBackLeft), 0.25f)
                      && Near(Vector3Distance(first.topFrontRight, first.topBackRight), 0.25f),
                "door slab front and back faces stay separated by door thickness");
    }
}

void TestSectorDoorSlabMeshDataHasStableAttributes()
{
    game::SectorDoorRender render;
    render.width = 2.0f;
    render.height = 1.5f;
    render.thickness = 0.25f;
    render.tint = Color{180, 210, 240, 255};

    const game::SectorDoorSlabMeshData first = game::BuildSectorDoorSlabMeshData(render);
    const game::SectorDoorSlabMeshData second = game::BuildSectorDoorSlabMeshData(render);

    Check(first.vertices.size() == 24 && first.indices.size() == 36,
            "door slab mesh emits duplicated face vertices and triangle indices");
    Check(first.vertices.size() == second.vertices.size() && first.indices.size() == second.indices.size(),
            "door slab mesh generation is stable for unchanged render data");

    bool finiteAttributes = true;
    bool stableAttributes = true;
    bool colorsNeutral = true;
    for (size_t i = 0; i < first.vertices.size(); ++i) {
        const game::SectorDoorSlabMeshVertex& vertex = first.vertices[i];
        const game::SectorDoorSlabMeshVertex& stableVertex = second.vertices[i];
        finiteAttributes = finiteAttributes
                && game::IsFiniteVector3(vertex.position)
                && game::IsFiniteVector3(vertex.normal)
                && game::IsFiniteVector2(vertex.uv);
        stableAttributes = stableAttributes
                && Near(vertex.position, stableVertex.position)
                && Near(vertex.normal, stableVertex.normal)
                && Near(vertex.uv, stableVertex.uv)
                && SameColor(vertex.color, stableVertex.color);
        colorsNeutral = colorsNeutral && SameColor(vertex.color, WHITE);
    }
    Check(finiteAttributes, "door slab mesh positions normals and UVs are finite");
    Check(stableAttributes, "door slab mesh vertex attributes are stable");
    Check(colorsNeutral, "door slab mesh starts with neutral static lighting vertex colors");

    bool indicesValid = true;
    bool stableIndices = true;
    for (size_t i = 0; i < first.indices.size(); ++i) {
        indicesValid = indicesValid && first.indices[i] < first.vertices.size();
        stableIndices = stableIndices && first.indices[i] == second.indices[i];
    }
    Check(indicesValid && stableIndices, "door slab mesh indices are valid and stable");

    const Vector3 expectedNormals[6] = {
            Vector3{0.0f, 0.0f, 1.0f},
            Vector3{0.0f, 0.0f, -1.0f},
            Vector3{1.0f, 0.0f, 0.0f},
            Vector3{-1.0f, 0.0f, 0.0f},
            Vector3{0.0f, 1.0f, 0.0f},
            Vector3{0.0f, -1.0f, 0.0f}};
    const Vector2 expectedUvMax[6] = {
            Vector2{render.width, render.height},
            Vector2{render.width, render.height},
            Vector2{render.thickness, render.height},
            Vector2{render.thickness, render.height},
            Vector2{render.width, render.thickness},
            Vector2{render.width, render.thickness}};
    bool faceAttributesMatch = true;
    for (size_t face = 0; face < 6; ++face) {
        const size_t base = face * 4;
        for (size_t corner = 0; corner < 4; ++corner) {
            faceAttributesMatch = faceAttributesMatch
                    && Near(first.vertices[base + corner].normal, expectedNormals[face]);
        }
        faceAttributesMatch = faceAttributesMatch
                && Near(first.vertices[base + 0].uv, Vector2{0.0f, expectedUvMax[face].y})
                && Near(first.vertices[base + 1].uv, expectedUvMax[face])
                && Near(first.vertices[base + 2].uv, Vector2{expectedUvMax[face].x, 0.0f})
                && Near(first.vertices[base + 3].uv, Vector2{0.0f, 0.0f});
    }
    Check(faceAttributesMatch, "door slab mesh duplicates expected per-face normals and UV scales");
}

void TestSectorDoorFaceUvsAffectOnlySelectedFace()
{
    game::SectorDoorRender baseRender;
    baseRender.width = 2.0f;
    baseRender.height = 1.5f;
    baseRender.thickness = 0.25f;

    game::SectorDoorRender editedRender = baseRender;
    game::DoorFaceUv(editedRender.faceUvs, game::SectorDoorFace::Front).scale = {2.0f, 3.0f};
    game::DoorFaceUv(editedRender.faceUvs, game::SectorDoorFace::Front).offset = {0.25f, 0.5f};

    const game::SectorDoorSlabMeshData base = game::BuildSectorDoorSlabMeshData(baseRender);
    const game::SectorDoorSlabMeshData edited = game::BuildSectorDoorSlabMeshData(editedRender);

    Check(base.vertices.size() == edited.vertices.size() && base.indices.size() == edited.indices.size(),
          "door UV edits preserve mesh vertex and index counts");

    bool frontChanged = false;
    bool otherFacesUnchanged = true;
    bool positionsUnchanged = true;
    bool normalsUnchanged = true;
    for (size_t i = 0; i < base.vertices.size(); ++i) {
        positionsUnchanged = positionsUnchanged && Near(base.vertices[i].position, edited.vertices[i].position);
        normalsUnchanged = normalsUnchanged && Near(base.vertices[i].normal, edited.vertices[i].normal);
        if (i < 4) {
            frontChanged = frontChanged || !Near(base.vertices[i].uv, edited.vertices[i].uv);
        } else {
            otherFacesUnchanged = otherFacesUnchanged && Near(base.vertices[i].uv, edited.vertices[i].uv);
        }
    }

    Check(frontChanged && otherFacesUnchanged,
          "door face UV edits affect only the selected face vertices");
    Check(positionsUnchanged && normalsUnchanged,
          "door face UV edits do not change visual geometry positions or normals");
    Check(Near(edited.vertices[0].uv, Vector2{0.25f, 5.0f})
                  && Near(edited.vertices[1].uv, Vector2{4.25f, 5.0f})
                  && Near(edited.vertices[2].uv, Vector2{4.25f, 0.5f})
                  && Near(edited.vertices[3].uv, Vector2{0.25f, 0.5f}),
          "door front face UV scale and offset are applied after base face dimensions");
}

void TestSectorDoorFaceUvHelpers()
{
    game::SectorDoorFaceUvSet uvs;
    game::SectorDoorRender render;
    render.width = 4.0f;
    render.height = 2.0f;
    render.thickness = 0.5f;

    std::string error;
    game::DoorFaceUv(uvs, game::SectorDoorFace::Front).scale = {2.0f, 3.0f};
    game::DoorFaceUv(uvs, game::SectorDoorFace::Front).offset = {5.0f, 7.0f};
    Check(game::FitSectorDoorFaceUv(
                  uvs,
                  game::SectorDoorFace::Front,
                  game::SectorDoorUvFitMode::Width,
                  render,
                  &error)
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Front).scale, Vector2{0.25f, 3.0f})
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Front).offset, Vector2{0.0f, 7.0f}),
          "door Fit Width changes only selected face U scale and offset");

    game::DoorFaceUv(uvs, game::SectorDoorFace::Front).offset.x = 3.0f;
    game::DoorFaceUv(uvs, game::SectorDoorFace::Front).offset.y = 4.0f;
    Check(game::FitSectorDoorFaceUv(
                  uvs,
                  game::SectorDoorFace::Front,
                  game::SectorDoorUvFitMode::Height,
                  render,
                  &error)
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Front).scale, Vector2{0.25f, 0.5f})
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Front).offset, Vector2{3.0f, 0.0f}),
          "door Fit Height changes only selected face V scale and offset");

    const game::SectorDoorFace fitFaces[] = {
            game::SectorDoorFace::Front,
            game::SectorDoorFace::Back,
            game::SectorDoorFace::Left,
            game::SectorDoorFace::Right,
            game::SectorDoorFace::Top,
            game::SectorDoorFace::Bottom};
    bool fitBothSpansOnce = true;
    bool fitBothOffsetsReset = true;
    for (game::SectorDoorFace face : fitFaces) {
        game::DoorFaceUv(uvs, face).scale = {2.0f, 3.0f};
        game::DoorFaceUv(uvs, face).offset = {4.0f, 5.0f};
        fitBothSpansOnce = fitBothSpansOnce
                && game::FitSectorDoorFaceUv(uvs, face, game::SectorDoorUvFitMode::Both, render, &error);
        const Vector2 baseUvSpan = game::SectorDoorFaceBaseUvSpan(render, face);
        const game::SectorDoorFaceUv& uv = game::DoorFaceUv(uvs, face);
        fitBothSpansOnce = fitBothSpansOnce
                && Near(baseUvSpan.x * uv.scale.x, 1.0f)
                && Near(baseUvSpan.y * uv.scale.y, 1.0f);
        fitBothOffsetsReset = fitBothOffsetsReset && Near(uv.offset, Vector2{0.0f, 0.0f});
    }
    Check(fitBothSpansOnce,
          "door Fit Both produces one final texture repeat on every face");
    Check(fitBothOffsetsReset,
          "door Fit Both resets both offsets on every face");

    game::DoorFaceUv(uvs, game::SectorDoorFace::Front).scale = {2.0f, 3.0f};
    game::DoorFaceUv(uvs, game::SectorDoorFace::Front).offset = {4.0f, 5.0f};
    Check(game::CopySectorDoorFaceUv(uvs, game::SectorDoorFace::Front, game::SectorDoorFace::Back)
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Back).scale, Vector2{2.0f, 3.0f})
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Back).offset, Vector2{4.0f, 5.0f}),
          "door Copy From Front copies front UVs to selected face");

    Check(game::ApplySectorDoorFaceUvToAll(uvs, game::SectorDoorFace::Back)
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Bottom).scale, Vector2{2.0f, 3.0f})
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Left).offset, Vector2{4.0f, 5.0f}),
          "door Apply To All copies selected UVs to every face");

    Check(game::ResetSectorDoorFaceUv(uvs, game::SectorDoorFace::Back)
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Back).scale, Vector2{1.0f, 1.0f})
                  && Near(game::DoorFaceUv(uvs, game::SectorDoorFace::Back).offset, Vector2{0.0f, 0.0f}),
          "door Reset Face restores default scale and offset");
}

void TestSectorDoorFitBothMeshUvsSpanOnce()
{
    game::SectorDoorRender render;
    render.width = 4.0f;
    render.height = 2.0f;
    render.thickness = 0.5f;

    const game::SectorDoorSlabMeshData defaultMesh = game::BuildSectorDoorSlabMeshData(render);
    Check(Near(DoorMeshFaceUvSpan(defaultMesh, game::SectorDoorFace::Front), Vector2{4.0f, 2.0f})
                  && Near(DoorMeshFaceUvSpan(defaultMesh, game::SectorDoorFace::Right), Vector2{0.5f, 2.0f})
                  && Near(DoorMeshFaceUvSpan(defaultMesh, game::SectorDoorFace::Top), Vector2{4.0f, 0.5f}),
          "default door face UVs keep generated base spans");

    const game::SectorDoorFace fitFaces[] = {
            game::SectorDoorFace::Front,
            game::SectorDoorFace::Back,
            game::SectorDoorFace::Left,
            game::SectorDoorFace::Right,
            game::SectorDoorFace::Top,
            game::SectorDoorFace::Bottom};
    bool fittedFaceSpansOnce = true;
    bool otherFacesUnchanged = true;
    for (game::SectorDoorFace fittedFace : fitFaces) {
        game::SectorDoorRender fittedRender = render;
        std::string error;
        fittedFaceSpansOnce = fittedFaceSpansOnce
                && game::FitSectorDoorFaceUv(
                        fittedRender.faceUvs,
                        fittedFace,
                        game::SectorDoorUvFitMode::Both,
                        fittedRender,
                        &error);
        const game::SectorDoorSlabMeshData fittedMesh = game::BuildSectorDoorSlabMeshData(fittedRender);
        fittedFaceSpansOnce = fittedFaceSpansOnce
                && Near(DoorMeshFaceUvSpan(fittedMesh, fittedFace), Vector2{1.0f, 1.0f});
        for (game::SectorDoorFace otherFace : fitFaces) {
            if (game::SectorDoorFaceIndex(otherFace) == game::SectorDoorFaceIndex(fittedFace)) {
                continue;
            }
            otherFacesUnchanged = otherFacesUnchanged
                    && DoorMeshFaceUvsMatch(fittedMesh, defaultMesh, otherFace);
        }
    }

    Check(fittedFaceSpansOnce,
          "door Fit Both produces a final 1x1 UV span on each generated mesh face");
    Check(otherFacesUnchanged,
          "door Fit Both on one face leaves other generated mesh face UVs unchanged");
}

void TestSpawnPlacedDoorCopiesFaceUvsWithoutChangingPhysicalState()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.width = 2.0f;
    door.height = 1.5f;
    door.thickness = 0.25f;
    game::DoorFaceUv(door.faceUvs, game::SectorDoorFace::Top).scale = {3.0f, 4.0f};
    game::DoorFaceUv(door.faceUvs, game::SectorDoorFace::Top).offset = {0.5f, 0.75f};
    map.runtimeObjects.push_back(MakePlacedDoor(41, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountDoorObjects(world) == 1, "door face UV fixture spawns one door entity");
    const engine::Entity entity = state.placedObjectEntities[0].entity;
    const game::SectorDoorRender& render = world.Get<game::SectorDoorRender>(entity);
    const game::SectorDoorCollider& collider = world.Get<game::SectorDoorCollider>(entity);
    std::vector<game::SectorDoorShadowCaster> casters;
    game::CollectSectorDoorShadowCasters(world, casters);

    Check(Near(game::DoorFaceUv(render.faceUvs, game::SectorDoorFace::Top).scale, Vector2{3.0f, 4.0f})
                  && Near(game::DoorFaceUv(render.faceUvs, game::SectorDoorFace::Top).offset, Vector2{0.5f, 0.75f}),
          "spawned door render component copies authored face UVs");
    Check(Near(render.width, 2.0f)
                  && Near(render.height, 1.5f)
                  && Near(render.thickness, 0.25f)
                  && Near(collider.halfExtents, Vector2{1.0f, 0.125f})
                  && casters.size() == 1
                  && Near(casters[0].width, render.width + game::kSectorDoorShadowCasterHorizontalSealMarginWorld * 2.0f)
                  && Near(casters[0].height, render.height + game::kSectorDoorShadowCasterVerticalSealMarginWorld * 2.0f),
          "door face UVs do not change render dimensions collider dimensions or shadow caster margins");
}

void TestSectorDoorSlabModelMatrixPreservesResolvedBasis()
{
    for (float normalOffset : {0.0f, 0.25f}) {
        engine::World world;
        engine::AssetManager assets;
        game::SectorRuntimeObjectState state;
        game::SectorTopologyMap map = MakeDoorPortalMap();
        game::SectorPlacedDoor door = MakeDoorOnPortal();
        door.width = 2.0f;
        door.height = 1.5f;
        door.thickness = 0.25f;
        door.normalOffset = normalOffset;
        map.runtimeObjects.push_back(MakePlacedDoor(40, door));

        game::RefreshSectorRuntimeObjectMapData(state, map);
        game::SpawnPlacedRuntimeObjects(world, assets, state, map);
        Check(CountDoorObjects(world) == 1, "door model matrix fixture spawns one door entity");

        const engine::Entity entity = state.placedObjectEntities[0].entity;
        const game::SectorObjectTransform& transform = world.Get<game::SectorObjectTransform>(entity);
        const game::SectorDoorResolvedAnchor& anchor = world.Get<game::SectorDoorResolvedAnchor>(entity);
        const game::SectorDoorRender& render = world.Get<game::SectorDoorRender>(entity);
        const Matrix model = game::BuildSectorDoorSlabModelMatrix(transform, anchor, render);

        const Vector3 origin = Vector3Transform(Vector3{}, model);
        const Vector3 localX = Vector3Subtract(Vector3Transform(Vector3{1.0f, 0.0f, 0.0f}, model), origin);
        const Vector3 localY = Vector3Subtract(Vector3Transform(Vector3{0.0f, 1.0f, 0.0f}, model), origin);
        const Vector3 localZ = Vector3Subtract(Vector3Transform(Vector3{0.0f, 0.0f, 1.0f}, model), origin);

        Check(Near(origin, transform.position)
                      && Near(localX, Vector3{anchor.tangent.x, 0.0f, anchor.tangent.y})
                      && Near(localY, Vector3{0.0f, 1.0f, 0.0f})
                      && Near(localZ, Vector3{anchor.normal.x, 0.0f, anchor.normal.y}),
                "door slab model matrix maps local axes to resolved tangent up and normal basis");

        const game::SectorDoorSlabMeshData mesh = game::BuildSectorDoorSlabMeshData(render);
        const game::SectorDoorSlabGeometry slab = game::BuildSectorDoorSlabGeometry(transform, anchor, render);
        Check(mesh.vertices.size() >= 8
                      && Near(Vector3Transform(mesh.vertices[0].position, model), slab.bottomFrontLeft)
                      && Near(Vector3Transform(mesh.vertices[1].position, model), slab.bottomFrontRight)
                      && Near(Vector3Transform(mesh.vertices[2].position, model), slab.topFrontRight)
                      && Near(Vector3Transform(mesh.vertices[3].position, model), slab.topFrontLeft)
                      && Near(Vector3Transform(mesh.vertices[4].position, model), slab.bottomBackRight)
                      && Near(Vector3Transform(mesh.vertices[5].position, model), slab.bottomBackLeft)
                      && Near(Vector3Transform(mesh.vertices[6].position, model), slab.topBackLeft)
                      && Near(Vector3Transform(mesh.vertices[7].position, model), slab.topBackRight),
                "door slab model matrix matches existing world-space slab placement");
    }
}

game::SectorBakedObjectLightProbeRuntimeData MakeDoorProbeRuntimeData(
        Vector3 firstPosition,
        Vector3 firstLighting,
        Vector3 secondPosition,
        Vector3 secondLighting)
{
    game::SectorBakedObjectLightProbeRuntimeData probes;
    game::SectorBakedObjectLightProbe first;
    first.sectorId = 10;
    first.position = firstPosition;
    for (Vector3& face : first.ambientCube) {
        face = firstLighting;
    }
    probes.probes.push_back(first);

    game::SectorBakedObjectLightProbe second;
    second.sectorId = 10;
    second.position = secondPosition;
    for (Vector3& face : second.ambientCube) {
        face = secondLighting;
    }
    probes.probes.push_back(second);

    probes.sectorRanges.push_back(game::SectorBakedObjectLightProbeSectorRange{10, 0, 2});
    return probes;
}

game::SectorDoorResolvedAnchor MakeStaticLightingDoorAnchor()
{
    game::SectorDoorResolvedAnchor anchor;
    anchor.frontSectorId = 10;
    anchor.backSectorId = 20;
    anchor.tangent = Vector2{1.0f, 0.0f};
    anchor.normal = Vector2{0.0f, 1.0f};
    return anchor;
}

game::SectorDoorRender MakeStaticLightingDoorRender()
{
    game::SectorDoorRender render;
    render.width = 4.0f;
    render.height = 1.0f;
    render.thickness = 0.25f;
    render.tint = Color{64, 128, 192, 255};
    return render;
}

void TestSectorDoorStaticLightingColorsSamplePerVertexProbes()
{
    const game::SectorObjectTransform transform{Vector3{}, 0.0f};
    const game::SectorObject object{10, true};
    const game::SectorDoorResolvedAnchor anchor = MakeStaticLightingDoorAnchor();
    const game::SectorDoorRender render = MakeStaticLightingDoorRender();
    const game::SectorDoorSlabMeshData mesh = game::BuildSectorDoorSlabMeshData(render);
    const Matrix model = game::BuildSectorDoorSlabModelMatrix(transform, anchor, render);
    const Vector3 leftVertex = Vector3Transform(mesh.vertices[0].position, model);
    const Vector3 rightVertex = Vector3Transform(mesh.vertices[1].position, model);
    const game::SectorBakedObjectLightProbeRuntimeData probes = MakeDoorProbeRuntimeData(
            leftVertex,
            Vector3{1.0f, 0.0f, 0.0f},
            rightVertex,
            Vector3{0.0f, 0.0f, 1.0f});

    std::vector<Vector3> colors;
    const bool built = game::BuildSectorDoorStaticLightingColors(
            mesh,
            transform,
            object,
            anchor,
            render,
            probes,
            nullptr,
            colors);

    Check(built && colors.size() == mesh.vertices.size(),
            "door static lighting helper emits one color per duplicated mesh vertex");
    Check(colors.size() >= 2
                  && colors[0].x > 0.94f
                  && colors[0].z < 0.07f
                  && colors[1].z > 0.94f
                  && colors[1].x < 0.07f,
            "door static lighting helper samples object probes at individual vertex positions");
    Check(colors.size() >= 8 && !Near(colors[0], colors[4]),
            "door duplicated face vertices can receive different static probe colors");
}

void TestSectorDoorStaticLightingColorsFallbackSafely()
{
    const game::SectorObjectTransform transform{Vector3{}, 0.0f};
    const game::SectorObject object{10, true};
    const game::SectorDoorResolvedAnchor anchor = MakeStaticLightingDoorAnchor();
    const game::SectorDoorRender render = MakeStaticLightingDoorRender();
    const game::SectorDoorSlabMeshData mesh = game::BuildSectorDoorSlabMeshData(render);
    const game::SectorBakedObjectLightProbeRuntimeData missingProbes;

    std::vector<Vector3> colors;
    Check(game::BuildSectorDoorStaticLightingColors(
                  mesh,
                  transform,
                  object,
                  anchor,
                  render,
                  missingProbes,
                  nullptr,
                  colors),
            "door static lighting helper handles missing probes without a map fallback");
    Check(!colors.empty() && Near(colors[0], Vector3{0.15f, 0.15f, 0.15f}),
            "door static lighting helper uses neutral fallback without probes or map");

    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorTopologySector* sector = game::FindSectorTopologySector(map, 10);
    Check(sector != nullptr, "door static lighting fallback fixture has sector");
    if (sector != nullptr) {
        sector->ambientColor = Color{64, 128, 255, 255};
        sector->ambientIntensity = 0.5f;
    }

    Check(game::BuildSectorDoorStaticLightingColors(
                  mesh,
                  transform,
                  object,
                  anchor,
                  render,
                  missingProbes,
                  &map,
                  colors),
            "door static lighting helper handles missing probes with a map fallback");
    Check(!colors.empty() && Near(colors[0], Vector3{32.0f / 255.0f, 64.0f / 255.0f, 0.5f}),
            "door static lighting helper uses sector ambient fallback when map is supplied");
}

void TestSectorDoorStaticLightingColorsDoNotMutateGeometry()
{
    const game::SectorObjectTransform transform{Vector3{}, 0.0f};
    const game::SectorObject object{10, true};
    const game::SectorDoorResolvedAnchor anchor = MakeStaticLightingDoorAnchor();
    const game::SectorDoorRender render = MakeStaticLightingDoorRender();
    const game::SectorDoorSlabMeshData before = game::BuildSectorDoorSlabMeshData(render);
    game::SectorDoorSlabMeshData mesh = before;
    const game::SectorBakedObjectLightProbeRuntimeData probes = MakeDoorProbeRuntimeData(
            Vector3{-2.0f, -0.5f, 0.125f},
            Vector3{1.0f, 0.0f, 0.0f},
            Vector3{2.0f, -0.5f, 0.125f},
            Vector3{0.0f, 0.0f, 1.0f});

    std::vector<Vector3> colors;
    game::BuildSectorDoorStaticLightingColors(
            mesh, transform, object, anchor, render, probes, nullptr, colors);

    bool geometryUnchanged = mesh.vertices.size() == before.vertices.size()
            && mesh.indices.size() == before.indices.size();
    for (size_t i = 0; geometryUnchanged && i < mesh.vertices.size(); ++i) {
        geometryUnchanged = Near(mesh.vertices[i].position, before.vertices[i].position)
                && Near(mesh.vertices[i].normal, before.vertices[i].normal)
                && Near(mesh.vertices[i].uv, before.vertices[i].uv)
                && SameColor(mesh.vertices[i].color, before.vertices[i].color);
    }
    for (size_t i = 0; geometryUnchanged && i < mesh.indices.size(); ++i) {
        geometryUnchanged = mesh.indices[i] == before.indices[i];
    }
    Check(geometryUnchanged,
            "door static lighting color helper does not mutate geometry positions normals UVs colors or indices");
}

engine::Entity AddDoorForDerivedState(
        engine::World& world,
        game::SectorDoorMotionType motionType,
        float openFraction,
        float openDistance);

void TestSectorDoorReceiverBoundsUseAnimatedSlabGeometry()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);
    const float openDistance = 1.5f;
    const float halfOpenOffset = game::SmootherStep01(0.5f) * EffectiveDoorOpenDistance(openDistance);
    const engine::Entity door = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            openDistance);
    world.Add(door, game::SectorObject{10, true});
    game::UpdateSectorDoorDerivedStateSystem(world);

    std::vector<game::SectorReceiverBounds> bounds;
    game::CollectSectorDoorReceiverBounds(world, bounds);
    Check(bounds.size() == 2
                  && bounds[0].sectorId == 10
                  && bounds[1].sectorId == 20,
            "door receiver bounds are appended for both portal sectors");
    Check(bounds.size() >= 2
                  && Near(bounds[0].min, Vector3{0.5f, 0.5f, -0.75f})
                  && Near(bounds[0].max, Vector3{0.75f, 2.0f, 1.25f})
                  && Near(bounds[1].min, bounds[0].min)
                  && Near(bounds[1].max, bounds[0].max),
            "door receiver bounds cover the current slab AABB including vertical extent");

    game::SectorDoorMotion& motion = world.Get<game::SectorDoorMotion>(door);
    motion.openFraction = 0.5f;
    motion.targetOpenFraction = 0.5f;
    game::UpdateSectorDoorDerivedStateSystem(world);

    bounds.clear();
    game::CollectSectorDoorReceiverBounds(world, bounds);
    Check(bounds.size() == 2
                  && Near(bounds[0].min, Vector3{0.5f, 0.5f + halfOpenOffset, -0.75f})
                  && Near(bounds[0].max, Vector3{0.75f, 2.0f + halfOpenOffset, 1.25f}),
            "door receiver bounds update after animated door transform changes");
}

void TestSectorDoorReceiverBoundsSkipNonRenderableDoors()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);

    const engine::Entity hiddenObject = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(hiddenObject, game::SectorObject{10, false});

    const engine::Entity disabledDoor = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(disabledDoor, game::SectorObject{10, true});
    world.Get<game::SectorDoor>(disabledDoor).enabled = false;

    const engine::Entity hiddenRender = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(hiddenRender, game::SectorObject{10, true});
    world.Get<game::SectorDoorRender>(hiddenRender).visible = false;

    const engine::Entity invalidShape = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(invalidShape, game::SectorObject{10, true});
    world.Get<game::SectorDoorRender>(invalidShape).width = 0.0f;

    game::UpdateSectorDoorDerivedStateSystem(world);

    std::vector<game::SectorReceiverBounds> bounds;
    game::CollectSectorDoorReceiverBounds(world, bounds);
    Check(bounds.empty(),
            "door receiver bounds skip hidden disabled render-hidden and invalid-shape doors");
}

void TestSectorDoorShadowCasterCollectionIncludesValidDoor()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);
    const engine::Entity door = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(door, game::SectorObject{10, true});
    game::UpdateSectorDoorDerivedStateSystem(world);

    std::vector<game::SectorDoorShadowCaster> casters;
    game::CollectSectorDoorShadowCasters(world, casters);
    const game::SectorObjectTransform& transform = world.Get<game::SectorObjectTransform>(door);
    const game::SectorDoorRender& render = world.Get<game::SectorDoorRender>(door);
    const game::SectorDoorCollider& collider = world.Get<game::SectorDoorCollider>(door);
    const float expectedShadowWidth =
            render.width + 2.0f * game::kSectorDoorShadowCasterHorizontalSealMarginWorld;
    const float expectedShadowHeight =
            render.height + 2.0f * game::kSectorDoorShadowCasterVerticalSealMarginWorld;

    Check(casters.size() == 1
                  && casters[0].placedObjectId == 1
                  && casters[0].entity == door
                  && Near(casters[0].position, transform.position)
                  && NearTranslation(casters[0].model, transform.position)
                  && Near(casters[0].width, expectedShadowWidth)
                  && Near(casters[0].height, expectedShadowHeight)
                  && Near(casters[0].thickness, render.thickness),
            "door shadow caster collection includes a valid renderable procedural door with seal margins");

    const game::SectorDoorSlabMeshData visualMesh = game::BuildSectorDoorSlabMeshData(render);
    const Matrix shadowModel = game::BuildSectorDoorShadowCasterModelMatrix(
            casters[0],
            render.width,
            render.height);
    Check(visualMesh.vertices.size() >= 8
                  && Near(Vector3Distance(
                                  Vector3Transform(visualMesh.vertices[0].position, shadowModel),
                                  Vector3Transform(visualMesh.vertices[1].position, shadowModel)),
                          expectedShadowWidth)
                  && Near(Vector3Distance(
                                  Vector3Transform(visualMesh.vertices[0].position, shadowModel),
                                  Vector3Transform(visualMesh.vertices[3].position, shadowModel)),
                          expectedShadowHeight)
                  && Near(Vector3Distance(
                                  Vector3Transform(visualMesh.vertices[0].position, shadowModel),
                                  Vector3Transform(visualMesh.vertices[5].position, shadowModel)),
                          render.thickness)
                  && NearTranslation(shadowModel, transform.position),
            "door shadow caster model expands local width and height only around the current slab center");
    Check(Near(collider.halfExtents.x * 2.0f, render.width)
                  && Near(collider.halfExtents.y * 2.0f, render.thickness)
                  && Near(collider.top - collider.bottom, render.height),
            "door shadow seal margins do not change visual render data or collision dimensions");
}

void TestSectorDoorShadowCasterCollectionSkipsNonRenderableDoors()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);

    const engine::Entity hiddenObject = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(hiddenObject, game::SectorObject{10, false});

    const engine::Entity disabledDoor = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(disabledDoor, game::SectorObject{10, true});
    world.Get<game::SectorDoor>(disabledDoor).enabled = false;

    const engine::Entity hiddenRender = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(hiddenRender, game::SectorObject{10, true});
    world.Get<game::SectorDoorRender>(hiddenRender).visible = false;

    const engine::Entity invalidShape = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    world.Add(invalidShape, game::SectorObject{10, true});
    world.Get<game::SectorDoorRender>(invalidShape).height = 0.0f;

    game::UpdateSectorDoorDerivedStateSystem(world);

    std::vector<game::SectorDoorShadowCaster> casters;
    game::CollectSectorDoorShadowCasters(world, casters);
    Check(casters.empty(),
            "door shadow caster collection skips hidden disabled render-hidden and invalid-shape doors");
}

void TestSectorDoorShadowCasterUsesAnimatedTransform()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);
    const float openDistance = 1.5f;
    const engine::Entity door = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideRight,
            0.0f,
            openDistance);
    world.Add(door, game::SectorObject{10, true});
    game::UpdateSectorDoorDerivedStateSystem(world);

    std::vector<game::SectorDoorShadowCaster> casters;
    game::CollectSectorDoorShadowCasters(world, casters);
    Check(casters.size() == 1, "closed door contributes one shadow caster");
    const Vector3 closedPosition = casters.empty() ? Vector3{} : casters[0].position;
    const game::SectorDoorResolvedAnchor& anchor = world.Get<game::SectorDoorResolvedAnchor>(door);
    const Vector3 slideDirection{anchor.tangent.x, 0.0f, anchor.tangent.y};

    game::SectorDoorMotion& motion = world.Get<game::SectorDoorMotion>(door);
    motion.openFraction = 0.5f;
    motion.targetOpenFraction = 0.5f;
    game::UpdateSectorDoorDerivedStateSystem(world);
    casters.clear();
    game::CollectSectorDoorShadowCasters(world, casters);
    const float halfOpenOffset = game::SmootherStep01(0.5f) * EffectiveDoorOpenDistance(openDistance);
    Check(casters.size() == 1
                  && Near(casters[0].position, Vector3{
                          closedPosition.x + slideDirection.x * halfOpenOffset,
                          closedPosition.y,
                          closedPosition.z + slideDirection.z * halfOpenOffset})
                  && NearTranslation(casters[0].model, casters[0].position)
                  && Near(casters[0].width,
                          world.Get<game::SectorDoorRender>(door).width
                                  + 2.0f * game::kSectorDoorShadowCasterHorizontalSealMarginWorld)
                  && Near(casters[0].height,
                          world.Get<game::SectorDoorRender>(door).height
                                  + 2.0f * game::kSectorDoorShadowCasterVerticalSealMarginWorld),
            "partially open door shadow caster uses current animated transform");

    motion.openFraction = 1.0f;
    motion.targetOpenFraction = 1.0f;
    game::UpdateSectorDoorDerivedStateSystem(world);
    casters.clear();
    game::CollectSectorDoorShadowCasters(world, casters);
    const float openOffset = EffectiveDoorOpenDistance(openDistance);
    Check(casters.size() == 1
                  && Near(casters[0].position, Vector3{
                          closedPosition.x + slideDirection.x * openOffset,
                          closedPosition.y,
                          closedPosition.z + slideDirection.z * openOffset})
                  && NearTranslation(casters[0].model, casters[0].position)
                  && !Near(casters[0].position, closedPosition),
            "open door shadow caster moves away from the closed authoring anchor");
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
    Check(Near(world.Get<game::SectorDoorMotion>(verticalEntity).travelAmount, 1.5f),
            "vertical door derives default open distance from portal height");
    Check(Near(world.Get<game::SectorDoorMotion>(horizontalEntity).travelAmount, 0.5f),
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
    game::ReserveSectorRuntimeObjectWorld(world, 4);

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

    const engine::Entity zeroAngularSpeed = world.CreateEntity();
    world.Add(zeroAngularSpeed, game::SectorDoor{4, true});
    world.Add(zeroAngularSpeed, game::SectorDoorMotion{
            game::SectorDoorMotionType::Swing,
            0.25f,
            1.0f,
            90.0f * DEG2RAD,
            0.0f,
            game::SectorDoorHinge::Start,
            game::SectorDoorSwingSide::Front});

    game::AdvanceSectorDoorMotionSystem(world, 0.5f);
    Check(Near(world.Get<game::SectorDoorMotion>(clamped).openFraction, 1.0f)
                  && Near(world.Get<game::SectorDoorMotion>(clamped).targetOpenFraction, 1.0f),
            "door motion clamps target and current open fraction to one");
    Check(Near(world.Get<game::SectorDoorMotion>(zeroSpeed).openFraction, 0.25f),
            "door motion with zero speed does not advance");
    Check(Near(world.Get<game::SectorDoorMotion>(disabled).openFraction, 0.25f),
            "disabled door motion does not advance");
    Check(Near(world.Get<game::SectorDoorMotion>(zeroAngularSpeed).openFraction, 0.25f),
            "swing door with zero angular speed does not advance");

    game::AdvanceSectorDoorMotionSystem(world, -1.0f);
    Check(Near(world.Get<game::SectorDoorMotion>(clamped).openFraction, 1.0f),
            "door motion ignores invalid negative dt");
}

game::SectorDoorResolvedAnchor MakeInteractionAnchor(Vector2 midpoint)
{
    game::SectorDoorResolvedAnchor anchor;
    anchor.lineDefId = 2;
    anchor.frontSectorId = 10;
    anchor.backSectorId = 20;
    anchor.frontSideDefId = 2;
    anchor.backSideDefId = 8;
    anchor.endpointA = Vector2{midpoint.x, midpoint.y - 0.5f};
    anchor.endpointB = Vector2{midpoint.x, midpoint.y + 0.5f};
    anchor.midpoint = midpoint;
    anchor.tangent = Vector2{0.0f, 1.0f};
    anchor.normal = Vector2{1.0f, 0.0f};
    anchor.openBottom = 0.0f;
    anchor.openTop = 2.0f;
    anchor.portalWidth = 1.0f;
    anchor.portalHeight = 2.0f;
    return anchor;
}

void TestSectorDoorAutoOpenSetsTargetFromPlayerRange()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);

    const engine::Entity autoDoor = world.CreateEntity();
    world.Add(autoDoor, game::SectorObjectTransform{Vector3{1.0f, 0.0f, 1.0f}, 0.0f});
    world.Add(autoDoor, game::SectorDoor{1, true});
    world.Add(autoDoor, MakeInteractionAnchor(Vector2{1.0f, 1.0f}));
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
    world.Add(manualDoor, MakeInteractionAnchor(Vector2{1.0f, 1.0f}));
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
    world.Add(disabledDoor, MakeInteractionAnchor(Vector2{1.0f, 1.0f}));
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
    world.Add(farther, MakeInteractionAnchor(Vector2{1.25f, 0.0f}));
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
    world.Add(nearest, MakeInteractionAnchor(Vector2{0.75f, 0.0f}));
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
    world.Add(autoDoor, MakeInteractionAnchor(Vector2{0.5f, 0.0f}));
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
    world.Add(door, MakeInteractionAnchor(Vector2{1.0f, 0.0f}));
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

void TestSectorSideSlidingDoorInteractUsesStablePortalAnchor()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);

    const engine::Entity door = world.CreateEntity();
    world.Add(door, game::SectorObjectTransform{Vector3{1.0f, 0.0f, 6.0f}, 0.0f});
    world.Add(door, game::SectorDoor{1, true});
    world.Add(door, MakeInteractionAnchor(Vector2{1.0f, 0.0f}));
    world.Add(door, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideRight,
            1.0f,
            1.0f,
            6.0f,
            1.0f});
    world.Add(door, game::SectorDoorInteraction{false, 1.5f, 2.0f});

    const bool closed = game::ToggleTargetedSectorDoorInteractionSystem(
            world,
            Vector3{0.0f, 0.0f, 0.0f},
            Vector3{1.0f, 0.0f, 0.0f});
    Check(closed && Near(world.Get<game::SectorDoorMotion>(door).targetOpenFraction, 0.0f),
            "side-sliding door closes from doorway even after slab center moved away");

    world.Get<game::SectorDoorMotion>(door).openFraction = 0.0f;
    const bool opened = game::ToggleTargetedSectorDoorInteractionSystem(
            world,
            Vector3{0.0f, 0.0f, 0.0f},
            Vector3{1.0f, 0.0f, 0.0f});
    Check(opened && Near(world.Get<game::SectorDoorMotion>(door).targetOpenFraction, 1.0f),
            "side-sliding door opens from the same stable doorway target");
}

void TestSectorSideSlidingDoorAutoOpenUsesStablePortalAnchor()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);

    const engine::Entity door = world.CreateEntity();
    world.Add(door, game::SectorObjectTransform{Vector3{1.0f, 0.0f, 6.0f}, 0.0f});
    world.Add(door, game::SectorDoor{1, true});
    world.Add(door, MakeInteractionAnchor(Vector2{1.0f, 0.0f}));
    world.Add(door, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideLeft,
            1.0f,
            0.0f,
            6.0f,
            1.0f});
    world.Add(door, game::SectorDoorInteraction{true, 1.5f, 1.25f});

    game::UpdateSectorDoorAutoOpenSystem(world, Vector3{0.25f, 0.0f, 0.0f});
    Check(Near(world.Get<game::SectorDoorMotion>(door).targetOpenFraction, 1.0f),
            "side-sliding auto-open uses doorway proximity instead of moved slab center");

    game::UpdateSectorDoorAutoOpenSystem(world, Vector3{3.0f, 0.0f, 0.0f});
    Check(Near(world.Get<game::SectorDoorMotion>(door).targetOpenFraction, 0.0f),
            "side-sliding auto-open closes when player leaves stable doorway range");
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

game::SectorDoorResolvedAnchor MakeRuntimeHorizontalDoorAnchorForDerivedState()
{
    game::SectorDoorResolvedAnchor anchor;
    anchor.lineDefId = 3;
    anchor.frontSectorId = 10;
    anchor.backSectorId = 20;
    anchor.frontSideDefId = 3;
    anchor.backSideDefId = 8;
    anchor.endpointA = Vector2{0.5f, 0.5f};
    anchor.endpointB = Vector2{0.0f, 0.5f};
    anchor.midpoint = Vector2{0.25f, 0.5f};
    anchor.tangent = Vector2{-1.0f, 0.0f};
    anchor.normal = Vector2{0.0f, 1.0f};
    anchor.openBottom = 0.0f;
    anchor.openTop = 1.0f;
    anchor.portalWidth = 0.5f;
    anchor.portalHeight = 1.0f;
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

engine::Entity AddHorizontalDoorForDerivedState(
        engine::World& world,
        game::SectorDoorMotionType motionType)
{
    game::SectorDoorRender render;
    render.width = 0.5f;
    render.height = 1.0f;
    render.thickness = 0.25f;
    render.normalOffset = 0.0f;
    render.visible = true;

    const engine::Entity entity = world.CreateEntity();
    world.Add(entity, game::SectorObjectTransform{});
    world.Add(entity, game::SectorDoor{1, true});
    world.Add(entity, MakeRuntimeHorizontalDoorAnchorForDerivedState());
    world.Add(entity, game::SectorDoorMotion{
            motionType,
            1.0f,
            1.0f,
            0.25f,
            1.0f});
    world.Add(entity, render);
    world.Add(entity, game::SectorDoorCollider{});
    world.Add(entity, game::SectorDoorPortalBlocker{3, 10, 20, 3, 8, true});
    return entity;
}

engine::Entity AddSwingDoorForDerivedState(
        engine::World& world,
        const game::SectorDoorResolvedAnchor& anchor,
        game::SectorDoorHinge hinge,
        game::SectorDoorSwingSide swingSide,
        float openFraction,
        float targetOpenFraction)
{
    game::SectorDoorRender render;
    render.width = anchor.portalWidth;
    render.height = anchor.portalHeight;
    render.thickness = 0.1f;
    render.normalOffset = 0.05f;
    render.visible = true;

    game::SectorDoorModelRender model;
    model.effectiveScale = 1.0f;
    model.actualWidth = render.width;
    model.actualHeight = render.height;
    model.actualThickness = render.thickness;

    const engine::Entity entity = world.CreateEntity();
    world.Add(entity, game::SectorObjectTransform{});
    world.Add(entity, game::SectorDoor{77, true});
    world.Add(entity, anchor);
    world.Add(entity, game::SectorDoorMotion{
            game::SectorDoorMotionType::Swing,
            openFraction,
            targetOpenFraction,
            90.0f * DEG2RAD,
            90.0f * DEG2RAD,
            hinge,
            swingSide});
    world.Add(entity, render);
    world.Add(entity, game::SectorDoorCollider{});
    world.Add(entity, game::SectorDoorPortalBlocker{
            anchor.lineDefId,
            anchor.frontSectorId,
            anchor.backSectorId,
            anchor.frontSideDefId,
            anchor.backSideDefId,
            true});
    world.Add(entity, model);
    return entity;
}

void TestSectorSwingDoorCanonicalPoseKeepsHingesAndChoosesSide()
{
    const game::SectorTopologyMap verticalMap = MakeDoorPortalMap();
    const game::SectorTopologyMap horizontalMap = MakeHorizontalDoorPortalMap();
    const game::SectorTopologyMap reversedMap = MakeReversedDoorPortalMap();
    const game::SectorDoorResolvedAnchor anchors[] = {
            game::ToSectorRuntimeDoorAnchor(game::ResolveSectorDoorAnchor(
                    verticalMap, MakeDoorOnPortal())),
            game::ToSectorRuntimeDoorAnchor(game::ResolveSectorDoorAnchor(
                    horizontalMap, MakeDoorOnHorizontalPortal())),
            game::ToSectorRuntimeDoorAnchor(game::ResolveSectorDoorAnchor(
                    reversedMap, MakeDoorOnReversedPortal()))};

    for (const game::SectorDoorResolvedAnchor& anchor : anchors) {
        for (game::SectorDoorHinge hinge : {
                     game::SectorDoorHinge::Start,
                     game::SectorDoorHinge::End}) {
            game::SectorDoorRender render;
            render.width = anchor.portalWidth;
            render.height = anchor.portalHeight;
            render.thickness = 0.1f;
            render.normalOffset = 0.075f;
            game::SectorDoorMotion motion{
                    game::SectorDoorMotionType::Swing,
                    0.0f,
                    1.0f,
                    90.0f * DEG2RAD,
                    90.0f * DEG2RAD,
                    hinge,
                    game::SectorDoorSwingSide::Front};
            const Vector2 endpoint = hinge == game::SectorDoorHinge::Start
                    ? anchor.endpointA
                    : anchor.endpointB;
            const Vector3 expectedHinge{
                    endpoint.x + anchor.normal.x * render.normalOffset,
                    anchor.openBottom,
                    endpoint.y + anchor.normal.y * render.normalOffset};
            for (float fraction : {0.0f, 0.5f, 1.0f}) {
                const game::SectorDoorSwingPose pose = game::BuildSectorDoorSwingPose(
                        anchor, motion, render, 1.0f, fraction);
                Check(Near(pose.hingePosition, expectedHinge)
                              && Near(Vector3Transform(Vector3{}, pose.leafMatrix), expectedHinge),
                      "swing pose keeps the selected hinge fixed for every portal orientation and fraction");
            }

            const game::SectorDoorSwingPose closed = game::BuildSectorDoorSwingPose(
                    anchor, motion, render, 1.0f, 0.0f);
            const game::SectorDoorSwingPose front = game::BuildSectorDoorSwingPose(
                    anchor, motion, render, 1.0f, 1.0f);
            motion.swingSide = game::SectorDoorSwingSide::Back;
            const game::SectorDoorSwingPose back = game::BuildSectorDoorSwingPose(
                    anchor, motion, render, 1.0f, 1.0f);
            const Vector2 frontDisplacement{
                    front.center.x - closed.center.x,
                    front.center.z - closed.center.z};
            const Vector2 backDisplacement{
                    back.center.x - closed.center.x,
                    back.center.z - closed.center.z};
            Check(Vector2DotProduct(frontDisplacement, anchor.normal) < 0.0f
                          && Vector2DotProduct(backDisplacement, anchor.normal) > 0.0f,
                  "front and back swing choices move the leaf center to the requested portal side");

            game::SectorDoorRender framedRender = render;
            framedRender.width = anchor.portalWidth * 0.7f;
            framedRender.height = anchor.portalHeight * 0.8f;
            framedRender.leafHingeToFrameCenter = anchor.portalWidth * 0.35f;
            framedRender.leafBottomOffset = anchor.portalHeight * 0.05f;
            framedRender.alignLeafToFrame = true;
            const Vector2 closedWidthAxis = hinge == game::SectorDoorHinge::Start
                    ? anchor.tangent
                    : Vector2{-anchor.tangent.x, -anchor.tangent.y};
            const Vector3 expectedFramedHinge{
                    anchor.midpoint.x
                            - closedWidthAxis.x
                                    * framedRender.leafHingeToFrameCenter
                            + anchor.normal.x * framedRender.normalOffset,
                    anchor.openBottom + framedRender.leafBottomOffset,
                    anchor.midpoint.y
                            - closedWidthAxis.y
                                    * framedRender.leafHingeToFrameCenter
                            + anchor.normal.y * framedRender.normalOffset};
            const Vector3 expectedFrameOrigin{
                    anchor.midpoint.x + anchor.normal.x * framedRender.normalOffset,
                    anchor.openBottom,
                    anchor.midpoint.y + anchor.normal.y * framedRender.normalOffset};
            const game::SectorDoorSwingPose framed = game::BuildSectorDoorSwingPose(
                    anchor, motion, framedRender, 2.0f, 0.5f);
            Check(Near(framed.hingePosition, expectedFramedHinge)
                          && Near(Vector3Transform(Vector3{}, framed.leafMatrix),
                                  expectedFramedHinge)
                          && Near(Vector3Transform(Vector3{}, framed.frameMatrix),
                                  expectedFrameOrigin)
                          && Near(framed.bottom, expectedFramedHinge.y)
                          && Near(framed.top,
                                  expectedFramedHinge.y + framedRender.height),
                  "framed swing pose preserves paired horizontal inset and vertical leaf offset for either hinge");
        }
    }
}

void TestSectorSwingDoorDerivedMatricesAndColliderAgree()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);
    const game::SectorDoorResolvedAnchor anchor = MakeRuntimeDoorAnchorForDerivedState();
    const engine::Entity entity = AddSwingDoorForDerivedState(
            world,
            anchor,
            game::SectorDoorHinge::End,
            game::SectorDoorSwingSide::Back,
            0.5f,
            0.5f);
    game::SectorDoorModelRender& model = world.Get<game::SectorDoorModelRender>(entity);
    model.effectiveScale = 2.0f;
    model.nominalWidth = anchor.portalWidth * 0.5f;
    model.nominalHeight = anchor.portalHeight * 0.5f;

    game::UpdateSectorDoorDerivedStateSystem(world);

    const game::SectorObjectTransform& transform =
            world.Get<game::SectorObjectTransform>(entity);
    const game::SectorDoorRender& render = world.Get<game::SectorDoorRender>(entity);
    const game::SectorDoorCollider& collider = world.Get<game::SectorDoorCollider>(entity);
    const game::SectorDoorModelRender& derivedModel =
            world.Get<game::SectorDoorModelRender>(entity);
    const Matrix slabMatrix = game::BuildSectorDoorSlabModelMatrix(
            transform, anchor, render);
    const Vector3 modelPanelCenter = Vector3Transform(
            Vector3{
                    derivedModel.nominalWidth * 0.5f,
                    derivedModel.nominalHeight * 0.5f,
                    0.0f},
            derivedModel.leafMatrix);
    Check(Near(modelPanelCenter, transform.position)
                  && Near(Vector3Transform(Vector3{}, slabMatrix), transform.position)
                  && Near(collider.center, Vector2{transform.position.x, transform.position.z}),
          "half-open model matrix, procedural slab matrix, transform, and collider share one center");
    Check(Near(collider.tangent, render.widthAxis)
                  && Near(collider.normal, render.thicknessAxis)
                  && Near(collider.bottom, anchor.openBottom)
                  && Near(collider.top, anchor.openBottom + render.height),
          "half-open model and procedural representations share collider axes and vertical interval");
    Check(!world.Get<game::SectorDoorPortalBlocker>(entity).blocksPortal,
          "partly open swing door conservatively unblocks portal visibility");

    game::SectorDoorMotion& motion = world.Get<game::SectorDoorMotion>(entity);
    motion.openFraction = game::kSectorDoorPortalBlockEpsilon;
    game::UpdateSectorDoorDerivedStateSystem(world);
    Check(world.Get<game::SectorDoorPortalBlocker>(entity).blocksPortal,
          "swing door remains visibility-blocking at the closed epsilon");
    motion.openFraction = game::kSectorDoorPortalBlockEpsilon + 0.0001f;
    game::UpdateSectorDoorDerivedStateSystem(world);
    Check(!world.Get<game::SectorDoorPortalBlocker>(entity).blocksPortal,
          "swing door stops visibility blocking beyond the closed epsilon");
}

void TestSectorSwingDoorFullyOpenClearsApertureButStillCollides()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 1);
    const game::SectorDoorResolvedAnchor anchor = MakeRuntimeDoorAnchorForDerivedState();
    const engine::Entity entity = AddSwingDoorForDerivedState(
            world,
            anchor,
            game::SectorDoorHinge::Start,
            game::SectorDoorSwingSide::Front,
            1.0f,
            1.0f);
    game::UpdateSectorDoorDerivedStateSystem(world);
    std::vector<game::SectorDynamicDoorCollider> colliders;
    game::CollectSectorDoorDynamicColliders(world, colliders);
    Check(colliders.size() == 1,
          "fully open swing leaf retains one active physical OBB");

    const Vector2 normal = anchor.normal;
    const game::SectorCollisionMoveState doorwayStart{
            Vector2{
                    anchor.midpoint.x - normal.x * 0.4f,
                    anchor.midpoint.y - normal.y * 0.4f},
            anchor.openBottom,
            anchor.frontSectorId,
            true};
    const game::SectorCollisionMoveResult doorwayStatic{
            Vector2{
                    anchor.midpoint.x + normal.x * 0.4f,
                    anchor.midpoint.y + normal.y * 0.4f},
            anchor.backSectorId,
            false,
            false,
            false};
    const game::SectorCollisionMoveResult throughDoor =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    doorwayStart,
                    doorwayStatic,
                    game::SectorCollisionMoveConfig{0.02f, 1.0f, 0.25f, 4},
                    colliders);
    Check(!throughDoor.hitWall && throughDoor.currentSectorId == anchor.backSectorId,
          "fully open fitted swing leaf clears the center of the portal aperture");

    const game::SectorDoorCollider& physical = world.Get<game::SectorDoorCollider>(entity);
    const game::SectorCollisionMoveState leafStart{
            Vector2{
                    physical.center.x - physical.tangent.x * anchor.portalWidth,
                    physical.center.y - physical.tangent.y * anchor.portalWidth},
            anchor.openBottom,
            anchor.frontSectorId,
            true};
    const game::SectorCollisionMoveResult leafStatic{
            physical.center,
            anchor.frontSectorId,
            false,
            false,
            false};
    const game::SectorCollisionMoveResult intoLeaf =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    leafStart,
                    leafStatic,
                    game::SectorCollisionMoveConfig{0.05f, 1.0f, 0.25f, 4},
                    colliders);
    Check(intoLeaf.hitWall,
          "fully open swing leaf still blocks movement at its rotated physical location");
}

void TestSectorSwingDoorClosingSweepReopensWithoutTunneling()
{
    const game::SectorDoorResolvedAnchor anchor = MakeRuntimeDoorAnchorForDerivedState();
    game::SectorDoorRender render;
    render.width = anchor.portalWidth * 0.7f;
    render.height = anchor.portalHeight * 0.8f;
    render.thickness = 0.1f;
    render.normalOffset = 0.05f;
    render.leafHingeToFrameCenter = anchor.portalWidth * 0.35f;
    render.leafBottomOffset = anchor.portalHeight * 0.05f;
    render.alignLeafToFrame = true;
    const auto runClosingStep = [&anchor, &render](
                                        Vector3 obstaclePosition,
                                        bool expectBlocked) {
        engine::World world;
        game::ReserveSectorRuntimeObjectWorld(world, 1);
        const engine::Entity entity = AddSwingDoorForDerivedState(
                world,
                anchor,
                game::SectorDoorHinge::Start,
                game::SectorDoorSwingSide::Front,
                1.0f,
                0.0f);
        world.Get<game::SectorDoorRender>(entity) = render;
        const game::SectorDoorPlayerObstacle obstacle{
                obstaclePosition,
                0.08f,
                1.0f};
        const bool changed = game::AdvanceSectorDoorMotionSystem(
                world, 2.0f, &obstacle);
        const game::SectorDoorMotion& motion = world.Get<game::SectorDoorMotion>(entity);
        if (expectBlocked) {
            Check(changed
                          && Near(motion.openFraction, 1.0f)
                          && Near(motion.targetOpenFraction, 1.0f),
                  "closing swing obstruction preserves the previous fraction and retargets open");
        } else {
            Check(changed
                          && Near(motion.openFraction, 0.0f)
                          && Near(motion.targetOpenFraction, 0.0f),
                  "closing swing advances when the player cylinder does not overlap its sweep");
        }
        return motion;
    };

    const game::SectorDoorMotion poseMotion{
            game::SectorDoorMotionType::Swing,
            1.0f,
            0.0f,
            90.0f * DEG2RAD,
            90.0f * DEG2RAD,
            game::SectorDoorHinge::Start,
            game::SectorDoorSwingSide::Front};
    const game::SectorDoorSwingPose current = game::BuildSectorDoorSwingPose(
            anchor, poseMotion, render, 1.0f, 1.0f);
    const game::SectorDoorSwingPose intermediate = game::BuildSectorDoorSwingPose(
            anchor, poseMotion, render, 1.0f, 0.5f);
    const game::SectorDoorSwingPose candidate = game::BuildSectorDoorSwingPose(
            anchor, poseMotion, render, 1.0f, 0.0f);

    runClosingStep(Vector3{current.center.x, anchor.openBottom, current.center.z}, true);
    const game::SectorDoorMotion blockedMotion = runClosingStep(
            Vector3{intermediate.center.x, anchor.openBottom, intermediate.center.z},
            true);
    runClosingStep(Vector3{candidate.center.x, anchor.openBottom, candidate.center.z}, true);
    runClosingStep(Vector3{100.0f, anchor.openBottom, 100.0f}, false);
    runClosingStep(Vector3{
            intermediate.center.x,
            anchor.openTop + 1.0f,
            intermediate.center.z}, false);

    game::SectorDoorAudio audio;
    audio.targetWasOpen = false;
    Check(game::UpdateSectorDoorAudioTransition(audio, blockedMotion)
                          == game::SectorDoorAudioEvent::Open
                  && game::UpdateSectorDoorAudioTransition(audio, blockedMotion)
                          == game::SectorDoorAudioEvent::None,
          "obstruction reversal creates one open audio transition without per-frame retriggering");
}

void TestSectorDoorNavigationHoldsAndSlidingObstruction()
{
    engine::World heldWorld;
    game::ReserveSectorRuntimeObjectWorld(heldWorld, 1);
    const engine::Entity heldDoor = AddDoorForDerivedState(
            heldWorld,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    heldWorld.Add(heldDoor, game::SectorDoorOpenControl{2});
    game::SectorDoorMotion& heldMotion =
            heldWorld.Get<game::SectorDoorMotion>(heldDoor);
    heldMotion.targetOpenFraction = 0.0f;
    game::AdvanceSectorDoorMotionSystem(heldWorld, 0.75f, nullptr);
    const float afterTwoHolders = heldMotion.openFraction;
    heldWorld.Get<game::SectorDoorOpenControl>(heldDoor)
            .navigationHolderCount = 1;
    game::AdvanceSectorDoorMotionSystem(heldWorld, 0.75f, nullptr);
    Check(afterTwoHolders > 0.0f && Near(heldMotion.openFraction, 1.0f),
          "two NPC holds combine with a closed ordinary target and one remaining hold keeps opening");
    heldWorld.Get<game::SectorDoorOpenControl>(heldDoor)
            .navigationHolderCount = 0;
    game::AdvanceSectorDoorMotionSystem(heldWorld, 1.5f, nullptr);
    Check(Near(heldMotion.openFraction, 0.0f),
          "door resumes its ordinary closed target after the final NPC hold releases");

    engine::World slideWorld;
    game::ReserveSectorRuntimeObjectWorld(slideWorld, 1);
    const engine::Entity slidingDoor = AddHorizontalDoorForDerivedState(
            slideWorld, game::SectorDoorMotionType::SlideLeft);
    game::UpdateSectorDoorDerivedStateSystem(slideWorld);
    game::SectorDoorMotion& slidingMotion =
            slideWorld.Get<game::SectorDoorMotion>(slidingDoor);
    slidingMotion.targetOpenFraction = 0.0f;
    const game::SectorDoorResolvedAnchor& anchor =
            slideWorld.Get<game::SectorDoorResolvedAnchor>(slidingDoor);
    const game::SectorDoorPlayerObstacle obstacle{
            {anchor.midpoint.x, anchor.openBottom, anchor.midpoint.y},
            0.15f,
            1.0f};
    game::AdvanceSectorDoorMotionSystem(slideWorld, 1.0f, &obstacle);
    Check(Near(slidingMotion.openFraction, 1.0f)
                  && Near(slidingMotion.targetOpenFraction, 1.0f),
          "closing sliding slab detects a cylinder in its swept volume and retargets open");
}

void TestNpcDoorTraversalStagesWaitsCrossesAndReleases()
{
    game::SectorTopologyMap map = MakeDoorPortalMap();
    for (game::SectorTopologyVertex& vertex : map.vertices) {
        vertex.x *= 2;
        vertex.y *= 2;
    }
    map.sectors[0].floorZ = 1.0f;
    map.sectors[1].floorZ = 1.0f;
    map.sectors[0].ceilingZ = 32.0f;
    map.sectors[1].ceilingZ = 32.0f;
    game::SectorPlacedDoor placedDoor = MakeDoorOnPortal();
    placedDoor.anchor.endpointAX = 128;
    placedDoor.anchor.endpointBX = 128;
    placedDoor.anchor.endpointBY = 128;
    placedDoor.speed = 2.0f;
    map.runtimeObjects.push_back(MakePlacedDoor(77, placedDoor));

    game::SectorCollisionWorld collisionWorld;
    std::string collisionError;
    Check(collisionWorld.BuildFromTopology(map, &collisionError),
          "door traversal collision fixture builds");
    game::SectorNavigationWorld navigation;
    navigation.Initialize();
    navigation.RequestRebuild();
    FinishNavigationBuild(navigation, map);
    if (navigation.State() != game::SectorNavigationState::Ready) {
        for (const game::SectorNavigationDiagnostic& diagnostic :
                navigation.Diagnostics()) {
            std::fprintf(stderr, "door traversal nav diagnostic: %s\n",
                    diagnostic.message.c_str());
        }
    }
    Check(navigation.State() == game::SectorNavigationState::Ready,
          "door traversal navigation fixture builds");

    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState runtimeObjects;
    game::RefreshSectorRuntimeObjectMapData(runtimeObjects, map);
    game::SpawnPlacedRuntimeObjects(world, assets, runtimeObjects, map);
    const engine::Entity npc = SpawnNavigationTestNpc(
            world, "door_walker", 501, {0.5f, 0.125f, 0.5f}, 10,
            2.0f, 4.0f);
    world.Get<game::NpcRuntimeInstance>(npc).canOpenDoors = true;

    game::NpcNavigationRuntime npcNavigation;
    game::InitializeNpcNavigationRuntime(world, navigation, npcNavigation);
    runtimeObjects.dynamicDoorColliders.clear();
    game::CollectSectorDoorDynamicColliders(
            world, runtimeObjects.dynamicDoorColliders);
    game::SynchronizeSectorNavigationDoorLinksSystem(
            world, navigation, runtimeObjects.dynamicDoorColliders);
    const game::NpcMoveRequestResult request = game::RequestNpcMove(
            world,
            navigation,
            collisionWorld,
            npcNavigation,
            "door_walker",
            {1.5f, 0.5f});
    if (!request.accepted) {
        std::fprintf(stderr, "door traversal request failed: %s (%s)\n",
                request.message.c_str(),
                game::SectorNavigationQueryStatusName(request.status));
    }
    Check(request.accepted,
          "capable NPC accepts a path through a closed typed door link");

    game::NpcDefinitionCatalog definitions;
    game::SectorBakedObjectLightProbeRuntimeData probes;
    const std::vector<game::SectorStaticModelCollider> staticColliders;
    bool observedWaiting = false;
    bool observedCrossing = false;
    bool observedHold = false;
    for (int frame = 0; frame < 400; ++frame) {
        constexpr float Dt = 0.05f;
        game::PrepareNpcDoorTraversalAndHoldsSystem(
                world,
                navigation,
                npcNavigation,
                runtimeObjects.dynamicDoorColliders,
                Dt);
        game::CollectNpcDoorObstacles(
                world, npcNavigation, runtimeObjects.doorObstacles);
        const bool doorChanged = game::AdvanceSectorDoorMotionSystem(
                world,
                Dt,
                runtimeObjects.doorObstacles.data(),
                runtimeObjects.doorObstacles.size());
        if (doorChanged) game::UpdateSectorDoorDerivedStateSystem(world);
        runtimeObjects.dynamicDoorColliders.clear();
        game::CollectSectorDoorDynamicColliders(
                world, runtimeObjects.dynamicDoorColliders);
        game::SynchronizeSectorNavigationDoorLinksSystem(
                world, navigation, runtimeObjects.dynamicDoorColliders);
        game::UpdateNpcNavigationAndLocomotionSystem(
                world,
                assets,
                navigation,
                npcNavigation,
                definitions,
                collisionWorld,
                runtimeObjects.dynamicDoorColliders,
                staticColliders,
                probes,
                map,
                Dt);
        const game::NpcNavigationRecord& record = npcNavigation.records[0];
        observedWaiting = observedWaiting
                || record.doorPhase
                        == game::NpcDoorTraversalPhase::WaitingForClearance;
        observedCrossing = observedCrossing
                || record.doorPhase == game::NpcDoorTraversalPhase::Crossing;
        observedHold = observedHold || record.holdsDoor;
        if (game::GetNpcMoveStatus(npcNavigation, "door_walker").phase
                == game::NpcMovePhase::Arrived) break;
    }
    game::PrepareNpcDoorTraversalAndHoldsSystem(
            world,
            navigation,
            npcNavigation,
            runtimeObjects.dynamicDoorColliders,
            0.05f);
    uint32_t finalHolders = 0;
    world.ForEach<game::SectorDoorOpenControl>(
            [&finalHolders](engine::Entity, game::SectorDoorOpenControl& control) {
                finalHolders += control.navigationHolderCount;
            });
    Check(game::GetNpcMoveStatus(npcNavigation, "door_walker").phase
                  == game::NpcMovePhase::Arrived
                  && observedWaiting && observedCrossing && observedHold
                  && finalHolders == 0,
          "NPC stages, holds, waits for physical clearance, crosses, and releases the door");

    const game::NpcMoveRequestResult returnRequest = game::RequestNpcMove(
            world,
            navigation,
            collisionWorld,
            npcNavigation,
            "door_walker",
            {0.5f, 0.5f});
    if (returnRequest.accepted) {
        game::NpcNavigationRecord& record = npcNavigation.records[0];
        record.doorId = 77;
        record.doorPhase = game::NpcDoorTraversalPhase::WaitingForClearance;
        record.holdsDoor = true;
        world.ForEach<game::SectorDoor, game::SectorDoorOpenControl>(
                [](engine::Entity, game::SectorDoor& door,
                        game::SectorDoorOpenControl& control) {
                    if (door.placedObjectId == 77) {
                        control.navigationHolderCount = 1;
                    }
                });
        const bool cancelled = game::CancelNpcMove(
                world,
                navigation,
                npcNavigation,
                "door_walker",
                returnRequest.requestId);
        uint32_t holdersAfterCancel = 0;
        world.ForEach<game::SectorDoorOpenControl>(
                [&holdersAfterCancel](engine::Entity,
                        game::SectorDoorOpenControl& control) {
                    holdersAfterCancel += control.navigationHolderCount;
                });
        Check(cancelled && holdersAfterCancel == 0,
              "movement cancellation releases its stable NPC door hold immediately");
    } else {
        Check(false, "return move for door-hold cancellation fixture is accepted");
    }
}

void TestSectorDoorDerivedStateUpdatesTransformAndCollider()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 7);
    const float verticalOpenDistance = 1.5f;
    const float verticalEffectiveOpenDistance = EffectiveDoorOpenDistance(verticalOpenDistance);
    const float verticalQuarterOffset = game::SmootherStep01(0.25f) * verticalEffectiveOpenDistance;
    const float verticalHalfOffset = game::SmootherStep01(0.5f) * verticalEffectiveOpenDistance;
    const float verticalThreeQuarterOffset = game::SmootherStep01(0.75f) * verticalEffectiveOpenDistance;

    const engine::Entity closed = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            verticalOpenDistance);
    const engine::Entity verticalQuarter = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.25f,
            verticalOpenDistance);
    const engine::Entity verticalHalf = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.5f,
            verticalOpenDistance);
    const engine::Entity verticalThreeQuarter = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            0.75f,
            verticalOpenDistance);
    const engine::Entity verticalOpen = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            1.0f,
            verticalOpenDistance);
    const engine::Entity rightOpen = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideRight,
            1.0f,
            0.5f);
    const engine::Entity tinyOpenDistance = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideVertical,
            1.0f,
            kExpectedDoorParkingEpsilonWorld * 0.5f);

    game::UpdateSectorDoorDerivedStateSystem(world);

    Check(Near(world.Get<game::SectorObjectTransform>(closed).position, Vector3{0.625f, 1.25f, 0.25f}),
            "closed door derived transform is centered on the portal slab");
    Check(Near(world.Get<game::SectorObjectTransform>(verticalQuarter).position,
                  Vector3{0.625f, 1.25f + verticalQuarterOffset, 0.25f})
                  && verticalQuarterOffset < 0.25f * verticalEffectiveOpenDistance,
            "quarter-open vertical slide eases in below linear distance");
    Check(Near(world.Get<game::SectorObjectTransform>(verticalHalf).position,
                  Vector3{0.625f, 1.25f + verticalHalfOffset, 0.25f}),
            "half-open vertical slide reaches half the effective open distance");
    Check(Near(world.Get<game::SectorObjectTransform>(verticalThreeQuarter).position,
                  Vector3{0.625f, 1.25f + verticalThreeQuarterOffset, 0.25f})
                  && verticalThreeQuarterOffset > 0.75f * verticalEffectiveOpenDistance
                  && verticalThreeQuarterOffset < verticalEffectiveOpenDistance,
            "three-quarter-open vertical slide eases out above linear distance below final offset");
    Check(Near(world.Get<game::SectorObjectTransform>(verticalOpen).position,
                  Vector3{0.625f, 2.75f - kExpectedDoorParkingEpsilonWorld, 0.25f}),
            "fully open vertical slide derived transform parks just inside the open distance");
    Check(Near(world.Get<game::SectorObjectTransform>(rightOpen).position,
                  Vector3{0.625f, 1.25f, 0.75f - kExpectedDoorParkingEpsilonWorld}),
            "fully open right slide derived transform parks just inside the open distance");
    Check(Near(world.Get<game::SectorObjectTransform>(tinyOpenDistance).position,
                  Vector3{0.625f, 1.25f + kExpectedDoorParkingEpsilonWorld * 0.5f, 0.25f}),
            "fully open door with tiny open distance does not park below zero translation");

    const game::SectorDoorCollider& collider = world.Get<game::SectorDoorCollider>(verticalHalf);
    Check(collider.enabled
                  && Near(collider.center, Vector2{0.625f, 0.25f})
                  && Near(collider.tangent, Vector2{0.0f, 1.0f})
                  && Near(collider.normal, Vector2{1.0f, 0.0f})
                  && Near(collider.halfExtents, Vector2{1.0f, 0.125f})
                  && Near(collider.bottom, 0.5f + verticalHalfOffset)
                  && Near(collider.top, 2.0f + verticalHalfOffset),
            "door derived collider stores current OBB footprint and vertical interval");
}

void TestSectorDoorHorizontalSlideMotionUsesResolvedTangent()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);

    const engine::Entity leftOpen = AddHorizontalDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideLeft);
    const engine::Entity rightOpen = AddHorizontalDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideRight);

    game::UpdateSectorDoorDerivedStateSystem(world);

    Check(Near(world.Get<game::SectorObjectTransform>(leftOpen).position,
                  Vector3{0.5f - kExpectedDoorParkingEpsilonWorld, 0.5f, 0.5f}),
          "fully open horizontal slide-left door parks just inside negative resolved tangent");
    Check(Near(world.Get<game::SectorObjectTransform>(rightOpen).position,
                  Vector3{kExpectedDoorParkingEpsilonWorld, 0.5f, 0.5f}),
          "fully open horizontal slide-right door parks just inside positive resolved tangent");
    Check(Near(world.Get<game::SectorDoorCollider>(leftOpen).normal, Vector2{0.0f, 1.0f})
                  && Near(world.Get<game::SectorDoorCollider>(rightOpen).normal, Vector2{0.0f, 1.0f}),
          "horizontal slide doors keep collider normals from the resolved front-to-back basis");
}

void TestSectorDoorDerivedStateUpdatesLeftSlideAndBlockerThreshold()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 3);
    const float leftOpenDistance = 0.5f;
    const float leftHalfOffset = game::SmootherStep01(0.5f) * EffectiveDoorOpenDistance(leftOpenDistance);

    const engine::Entity leftHalf = AddDoorForDerivedState(
            world,
            game::SectorDoorMotionType::SlideLeft,
            0.5f,
            leftOpenDistance);
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

    Check(Near(world.Get<game::SectorObjectTransform>(leftHalf).position,
                  Vector3{0.625f, 1.25f, 0.25f - leftHalfOffset}),
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
                  && Near(openCollider->bottom, 2.0f - kExpectedDoorParkingEpsilonWorld)
                  && Near(openCollider->top, 3.5f - kExpectedDoorParkingEpsilonWorld),
            "fully open dynamic door collider snapshot uses current moved vertical interval");
}

void TestSectorDoorDynamicColliderCollectionExcludesDisabledAndInvalidShapes()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);
    const float validOpenDistance = 0.5f;
    const float validHalfOffset = game::SmootherStep01(0.5f) * EffectiveDoorOpenDistance(validOpenDistance);

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
            validOpenDistance);

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
                  && Near(colliders[0].center, Vector2{0.625f, 0.25f + validHalfOffset})
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
    Check(result.positionXZ.x > moveState.positionXZ.x + 0.5f
                  && result.positionXZ.x <= 0.7f + 0.001f
                  && Near(result.positionXZ.y, 0.0f),
            "thin closed dynamic door stops swept movement at contact without rolling back to its start");
}

void TestSectorDoorDynamicCollisionPreservesTangentialSlide()
{
    const game::SectorCollisionMoveState moveState{
            Vector2{0.0f, -0.8f},
            0.0f,
            10,
            true};
    const game::SectorCollisionMoveResult staticResult{
            Vector2{1.0f, 0.8f},
            20,
            false,
            false,
            false};
    const std::vector<game::SectorDynamicDoorCollider> colliders{
            MakeDynamicDoorCollider(
                    Vector2{0.75f, 0.0f},
                    Vector2{1.0f, 0.125f},
                    0.0f,
                    2.0f)};

    const game::SectorCollisionMoveResult result =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    staticResult,
                    game::SectorCollisionMoveConfig{0.25f, 1.6f, 0.25f, 4},
                    colliders);

    Check(result.hitWall && result.currentSectorId == moveState.currentSectorId,
          "diagonal dynamic door impact reports contact and preserves the starting sector");
    Check(result.positionXZ.x <= 0.375f + 0.001f
                  && result.positionXZ.y > 0.7f,
          "diagonal dynamic door impact preserves tangential sliding along the leaf");
}

void TestSectorDoorDynamicCollisionRotatedSlide()
{
    game::SectorDynamicDoorCollider collider = MakeDynamicDoorCollider(
            Vector2{},
            Vector2{0.8f, 0.12f},
            0.0f,
            2.0f);
    const float angle = PI * 0.25f;
    collider.tangent = Vector2{std::cos(angle), std::sin(angle)};
    collider.normal = Vector2{-std::sin(angle), std::cos(angle)};
    constexpr float radius = 0.25f;
    const float contactDistance = collider.halfExtents.y + radius;
    const Vector2 start = Vector2Scale(
            collider.normal,
            -(contactDistance + 0.5f));
    const Vector2 destination = Vector2Add(
            Vector2Add(start, Vector2Scale(collider.normal, 1.0f)),
            Vector2Scale(collider.tangent, 0.8f));

    const game::SectorCollisionMoveResult result =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    game::SectorCollisionMoveState{start, 0.0f, 10, true},
                    game::SectorCollisionMoveResult{
                            destination, 10, false, false, false},
                    game::SectorCollisionMoveConfig{radius, 1.6f, 0.25f, 4},
                    std::vector<game::SectorDynamicDoorCollider>{collider});
    const Vector2 relative = Vector2Subtract(result.positionXZ, collider.center);
    const float normalDistance = Vector2DotProduct(relative, collider.normal);
    const float tangentDistance = Vector2DotProduct(relative, collider.tangent);

    Check(result.hitWall
                  && normalDistance <= -contactDistance + 0.001f
                  && tangentDistance > 0.7f,
          "rotated swing-door OBB preserves tangential movement at diagonal contact");
}

void TestSectorDoorDynamicCollisionContactAllowsTangentAndAwayMovement()
{
    const game::SectorDynamicDoorCollider collider = MakeDynamicDoorCollider(
            Vector2{0.75f, 0.0f},
            Vector2{1.0f, 0.125f},
            0.0f,
            2.0f);
    constexpr float contactX = 0.375f;
    const game::SectorCollisionMoveState moveState{
            Vector2{contactX, 0.0f},
            0.0f,
            10,
            true};
    const game::SectorCollisionMoveConfig config{0.25f, 1.6f, 0.25f, 4};

    const game::SectorCollisionMoveResult tangent =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    game::SectorCollisionMoveResult{
                            Vector2{contactX, 0.4f}, 10, false, false, false},
                    config,
                    std::vector<game::SectorDynamicDoorCollider>{collider});
    Check(Near(tangent.positionXZ, Vector2{contactX, 0.4f}),
          "dynamic door face contact permits tangential movement");

    const game::SectorCollisionMoveResult away =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    moveState,
                    game::SectorCollisionMoveResult{
                            Vector2{contactX - 0.4f, 0.0f}, 10, false, false, false},
                    config,
                    std::vector<game::SectorDynamicDoorCollider>{collider});
    Check(Near(away.positionXZ, Vector2{contactX - 0.4f, 0.0f}),
                  "dynamic door face contact permits movement away from the leaf");
}

void TestSectorDoorDynamicCollisionRoundedCornerSweep()
{
    const std::vector<game::SectorDynamicDoorCollider> colliders{
            MakeDynamicDoorCollider(
                    Vector2{},
                    Vector2{0.5f, 0.5f},
                    0.0f,
                    2.0f)};
    const game::SectorCollisionMoveConfig config{0.5f, 1.6f, 0.25f, 4};

    const Vector2 nearMissStart{1.1f, 0.9f};
    const Vector2 nearMissDestination{0.9f, 1.1f};
    const game::SectorCollisionMoveResult nearMiss =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    game::SectorCollisionMoveState{
                            nearMissStart, 0.0f, 10, true},
                    game::SectorCollisionMoveResult{
                            nearMissDestination, 10, false, false, false},
                    config,
                    colliders);
    Check(!nearMiss.hitWall
                  && Near(nearMiss.positionXZ, nearMissDestination),
          "rounded door OBB corner does not block a player-circle near miss");

    const Vector2 cornerStart{1.2f, 1.2f};
    const game::SectorCollisionMoveResult cornerHit =
            game::ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    game::SectorCollisionMoveState{
                            cornerStart, 0.0f, 10, true},
                    game::SectorCollisionMoveResult{
                            Vector2{0.6f, 0.6f}, 10, false, false, false},
                    config,
                    colliders);
    const Vector2 fromCorner = Vector2Subtract(
            cornerHit.positionXZ,
            Vector2{0.5f, 0.5f});
    Check(cornerHit.hitWall
                  && Vector2Length(fromCorner) >= config.radius - 0.001f,
          "true rounded door-corner impact maintains player-circle clearance");
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
    Check(result.hitWall
                  && result.positionXZ.x > moveState.positionXZ.x + 0.5f
                  && result.positionXZ.x <= 0.7f + 0.001f,
            "dynamic door crossing collision uses physical collider contact rather than portal blocker state");
}

void TestSectorDoorDynamicPortalBlockerCollectionBuildsDirectedVisibilityKeys()
{
    engine::World closedWorld;
    game::ReserveSectorRuntimeObjectWorld(closedWorld, 1);
    AddDoorForDerivedState(
            closedWorld,
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            1.5f);
    game::UpdateSectorDoorDerivedStateSystem(closedWorld);

    std::vector<game::RuntimePortalDynamicBlocker> closedBlockers;
    game::CollectSectorDoorDynamicPortalBlockers(closedWorld, closedBlockers);
    Check(closedBlockers.size() == 2,
          "closed door portal blocker collection emits both directed portal keys");
    Check(closedBlockers[0].lineDefId == 2
                  && closedBlockers[0].sideDefId == 2
                  && closedBlockers[0].fromSectorId == 10
                  && closedBlockers[0].toSectorId == 20
                  && closedBlockers[0].blocksPortal,
          "closed door portal blocker collection emits front-to-back key");
    Check(closedBlockers[1].lineDefId == 2
                  && closedBlockers[1].sideDefId == 8
                  && closedBlockers[1].fromSectorId == 20
                  && closedBlockers[1].toSectorId == 10
                  && closedBlockers[1].blocksPortal,
          "closed door portal blocker collection emits back-to-front key");

    engine::World openWorld;
    game::ReserveSectorRuntimeObjectWorld(openWorld, 1);
    AddDoorForDerivedState(
            openWorld,
            game::SectorDoorMotionType::SlideVertical,
            game::kSectorDoorPortalBlockEpsilon + 0.0001f,
            1.5f);
    game::UpdateSectorDoorDerivedStateSystem(openWorld);

    std::vector<game::RuntimePortalDynamicBlocker> openBlockers;
    game::CollectSectorDoorDynamicPortalBlockers(openWorld, openBlockers);
    Check(openBlockers.empty(),
          "partly open door portal blocker collection emits no visibility blockers");
}

void TestSpawnedDoorRuntimeUpdateRefreshesPortalBlockerCollection()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeDoorPortalMap();
    game::SectorPlacedDoor door = MakeDoorOnPortal();
    door.speed = 1.0f;
    door.openDistance = 1.0f;
    map.runtimeObjects.push_back(MakePlacedDoor(39, door));

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    std::vector<game::RuntimePortalDynamicBlocker> blockers;
    game::CollectSectorDoorDynamicPortalBlockers(world, blockers);
    Check(blockers.size() == 2,
          "spawned closed door contributes directed portal visibility blockers");

    const engine::Entity entity = state.placedObjectEntities[0].entity;
    world.Get<game::SectorDoorMotion>(entity).targetOpenFraction = 1.0f;
    game::UpdateSectorRuntimeObjects(world, assets, state, map, 0.25f);

    blockers.clear();
    game::CollectSectorDoorDynamicPortalBlockers(world, blockers);
    Check(blockers.empty(),
          "spawned door stops contributing portal blockers after runtime motion opens it");
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

void TestSpawnPlacedStaticModelCopiesAuthoredPayloadToEcs()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorPlacedRuntimeObject object;
    object.id = 18;
    object.kind = "static_model";
    object.position = Vector3{2.0f, 8.0f, 2.0f};
    object.yawRadians = 0.75f;
    object.staticModel.modelPath = "assets/models/props/missing_fixture.glb";
    object.staticModel.rotationXRadians = 0.25f;
    object.staticModel.rotationZRadians = -0.5f;
    object.staticModel.heightOffsetWorld = 0.625f;
    object.staticModel.scale = 1.75f;
    object.staticModel.collision = true;
    map.runtimeObjects.push_back(object);

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(CountSectorObjects(world) == 1,
            "assigned static prop spawns one sector object entity");
    Check(state.placedObjectEntities.size() == 1
                  && state.placedObjectEntities[0].placedObjectId == 18,
          "static prop stores placed object ID to entity mapping");
    Check(state.spawnedObjectCount == 1 && state.skippedObjectCount == 0,
          "static prop spawn records debug counts");
    Check(state.staticModelRequestedCount == 1
                  && state.staticModelPendingCount == 1
                  && state.staticModelReadyCount == 0
                  && state.staticModelFailedCount == 0,
          "assigned static prop reports its queued model request");

    const engine::Entity entity = state.placedObjectEntities[0].entity;
    Check(world.IsAlive(entity)
                  && world.Has<game::SectorStaticModel>(entity)
                  && world.Has<game::SectorStaticModelCollider>(entity)
                  && !world.Has<game::SectorObjectLighting>(entity),
          "collision-enabled static prop has model and collider data and does not use object probes");
    const game::SectorObjectTransform& transform =
            world.Get<game::SectorObjectTransform>(entity);
    Check(Near(transform.position, Vector3{0.25f, 1.625f, 0.25f}),
          "static prop converts authored floor position and adds world height offset");
    Check(Near(transform.yawRadians, 0.75f),
          "static prop copies authored yaw to ECS transform");
    Check(Near(transform.rotationXRadians, 0.25f)
                  && Near(transform.rotationZRadians, -0.5f),
          "static prop copies authored X and Z rotations to ECS transform");
    Check(!engine::IsNull(world.Get<game::SectorStaticModel>(entity).model),
          "assigned static prop stores a model asset handle");
    Check(world.Get<game::SectorStaticModel>(entity).placedObjectId == 18,
          "assigned static prop stores its stable placed-object ID");
    Check(Near(world.Get<game::SectorStaticModel>(entity).scale, 1.75f),
          "static prop copies authored uniform scale to ECS model data");
    Check(Near(world.Get<game::SectorStaticModel>(entity).environmentExposure, 0.35f),
          "indoor static prop clamps sky reflection exposure from containing-sector ambient");
    Check(world.Get<game::SectorObject>(entity).currentSectorId == 10,
          "static prop spawn assigns the containing sector used by portal culling");
    Check(!world.Has<game::SectorDoorCollider>(entity),
          "static prop does not participate in door collision");
    Check(!world.Get<game::SectorStaticModelCollider>(entity).resolved
                  && !world.Get<game::SectorStaticModelCollider>(entity).failed
                  && state.staticModelColliders.empty(),
          "pending collision-enabled static prop remains inactive until model bounds are ready");
    Check(!world.Has<game::SectorBillboardSprite>(entity),
          "static prop does not acquire billboard render components");
}

void TestSectorModelEnvironmentExposureFollowsSectorLighting()
{
    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorTopologySector& sector = map.sectors[0];
    sector.ambientColor = WHITE;

    sector.ambientIntensity = 0.01f;
    Check(Near(game::ComputeSectorModelEnvironmentExposure(map, sector.id), 0.08f),
          "model environment exposure clamps very dark indoor sectors");

    sector.ambientIntensity = 0.2f;
    Check(Near(game::ComputeSectorModelEnvironmentExposure(map, sector.id), 0.2f),
          "model environment exposure follows indoor sector luminance");

    sector.ambientIntensity = 0.8f;
    Check(Near(game::ComputeSectorModelEnvironmentExposure(map, sector.id), 0.35f),
          "model environment exposure clamps bright indoor sectors");

    sector.ceilingSky = true;
    Check(Near(game::ComputeSectorModelEnvironmentExposure(map, sector.id), 1.0f),
          "model environment exposure uses full reflections in sky sectors");
    Check(Near(game::ComputeSectorModelEnvironmentExposure(map, 9999), 0.15f),
          "model environment exposure uses the neutral missing-sector fallback");
}

void TestSpawnUnassignedStaticModelRemainsSelectableRuntimeEntity()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    map.sectors[0].ceilingSky = true;
    game::SectorPlacedRuntimeObject object;
    object.id = 19;
    object.kind = "static_model";
    object.position = Vector3{2.0f, 0.0f, 2.0f};
    map.runtimeObjects.push_back(object);

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);

    Check(state.placedObjectEntities.size() == 1
                  && world.IsAlive(state.placedObjectEntities[0].entity),
          "unassigned static prop still spawns a runtime entity");
    const engine::Entity entity = state.placedObjectEntities[0].entity;
    Check(engine::IsNull(world.Get<game::SectorStaticModel>(entity).model),
          "unassigned static prop stores a null model handle");
    Check(!world.Has<game::SectorStaticModelCollider>(entity),
          "default-off static prop omits the collision component");
    Check(Near(world.Get<game::SectorStaticModel>(entity).environmentExposure, 1.0f),
          "sky-sector static prop uses full environment exposure");
    Check(engine::IsNull(state.runtimeObjectAssetScope),
          "unassigned static prop does not request an asset scope");
    Check(state.staticModelUnassignedCount == 1
                  && state.placedObjectWarning.find("no model assigned") != std::string::npos,
          "unassigned static prop produces an editor diagnostic without being skipped");
}

void TestSpawnDynamicModelCopiesPlaybackAndLightingPayload()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorPlacedRuntimeObject object;
    object.id = 27;
    object.kind = "dynamic_model";
    object.position = Vector3{2.0f, 8.0f, 2.0f};
    object.yawRadians = 0.75f;
    object.dynamicModel.rotationXRadians = 0.25f;
    object.dynamicModel.rotationZRadians = -0.5f;
    object.dynamicModel.heightOffsetWorld = 0.625f;
    object.dynamicModel.scale = 1.75f;
    object.dynamicModel.animation = "Standard Walk";
    object.dynamicModel.loop = false;
    object.dynamicModel.animationSpeed = 1.5f;
    object.dynamicModel.shadowMode = game::SectorDynamicModelShadowMode::ProjectedSilhouette;
    map.runtimeObjects.push_back(object);

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    Check(state.placedObjectEntities.size() == 1,
          "unassigned dynamic prop still spawns one runtime entity");
    const engine::Entity entity = state.placedObjectEntities[0].entity;
    Check(world.Has<game::SectorDynamicModel>(entity)
                  && world.Has<engine::AnimatedModelInstance>(entity)
                  && world.Has<engine::AnimatedModelAnimator>(entity)
                  && world.Has<game::SectorObjectLighting>(entity),
          "dynamic prop owns reusable animation state and baked probe lighting data");
    const game::SectorObjectTransform& transform =
            world.Get<game::SectorObjectTransform>(entity);
    const game::SectorDynamicModel& dynamic = world.Get<game::SectorDynamicModel>(entity);
    const engine::AnimatedModelAnimator& animator =
            world.Get<engine::AnimatedModelAnimator>(entity);
    Check(Near(transform.position, Vector3{0.25f, 1.625f, 0.25f})
                  && Near(transform.yawRadians, 0.75f)
                  && Near(transform.rotationXRadians, 0.25f)
                  && Near(transform.rotationZRadians, -0.5f)
                  && Near(dynamic.scale, 1.75f)
                  && dynamic.shadowMode
                          == game::SectorDynamicModelShadowMode::ProjectedSilhouette,
          "dynamic prop copies the movable authored transform, scale, and shadow mode");
    Check(dynamic.requestedAnimation == "Standard Walk"
                  && !dynamic.animationResolved
                  && !animator.loop
                  && !animator.playing
                  && Near(animator.frame, 0.0f)
                  && Near(animator.speed, 1.5f),
          "non-looping editor dynamic prop is configured frozen on its first frame");
}

void TestSpawnNpcResolvesDefinitionAndIdlePlayback()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorPlacedRuntimeObject object;
    object.id = 31;
    object.kind = "npc";
    object.position = Vector3{2.0f, 8.0f, 2.0f};
    object.yawRadians = 0.5f;
    object.npc.definitionId = "runtime_test_npc";
    object.npc.instanceId = "guard_one";
    object.npc.scale = 1.4f;
    object.npc.shadowMode =
            game::SectorDynamicModelShadowMode::ProjectedSilhouette;
    map.runtimeObjects.push_back(object);

    game::RefreshSectorRuntimeObjectMapData(state, map);
    game::NpcDefinition definition = game::MakeDefaultNpcDefinition();
    definition.id = "runtime_test_npc";
    definition.name = "Runtime Test";
    definition.hostile = true;
    definition.canOpenDoors = false;
    definition.modelPath = "assets/models/characters/Zombie1.glb";
    game::GetNpcAction(definition, game::NpcAction::Idle).animation = "Idle";
    game::GetNpcAction(definition, game::NpcAction::Idle).animationSpeed = 1.25f;
    game::GetNpcAction(definition, game::NpcAction::Walk).movementSpeed = 2.0f;
    game::GetNpcAction(definition, game::NpcAction::Run).movementSpeed = 4.5f;
    definition.animationBlendSeconds = 0.35f;
    state.npcDefinitionCatalog = game::NpcDefinitionCatalog{};
    state.npcDefinitionCatalog.definitions.push_back(definition);

    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    Check(state.placedObjectEntities.size() == 1,
          "resolved NPC placement spawns one runtime entity");
    if (state.placedObjectEntities.empty()) return;
    const engine::Entity entity = state.placedObjectEntities[0].entity;
    Check(world.Has<game::NpcRuntimeInstance>(entity)
                  && world.Has<game::NpcAnimationState>(entity)
                  && world.Has<game::SectorObjectVisualOffset>(entity)
                  && world.Has<game::SectorDynamicModel>(entity)
                  && world.Has<engine::AnimatedModelInstance>(entity)
                  && world.Has<engine::AnimatedModelAnimator>(entity),
          "NPC runtime reuses animated dynamic-model rendering components");
    const game::NpcRuntimeInstance& npc =
            world.Get<game::NpcRuntimeInstance>(entity);
    const game::NpcAnimationState& npcAnimation =
            world.Get<game::NpcAnimationState>(entity);
    const game::SectorDynamicModel& dynamic =
            world.Get<game::SectorDynamicModel>(entity);
    const engine::AnimatedModelAnimator& animator =
            world.Get<engine::AnimatedModelAnimator>(entity);
    const game::SectorObjectTransform& transform =
            world.Get<game::SectorObjectTransform>(entity);
    Check(npc.definitionId == "runtime_test_npc"
                  && npc.instanceId == "guard_one"
                  && npc.action == game::NpcAction::Idle
                  && npc.hostile
                  && !npc.canOpenDoors
                  && Near(npc.walkSpeed, 2.0f)
                  && Near(npc.runSpeed, 4.5f)
                  && Near(npcAnimation.blendSeconds, 0.35f)
                  && Near(npcAnimation.animationSpeeds[
                                  static_cast<size_t>(game::NpcAction::Idle)], 1.25f),
          "NPC runtime component copies resolved identity and gameplay definition fields");
    Check(dynamic.requestedAnimation == "Idle"
                  && Near(dynamic.scale, 1.4f)
                  && dynamic.shadowMode
                          == game::SectorDynamicModelShadowMode::ProjectedSilhouette
                  && animator.loop
                  && animator.playing
                  && Near(animator.speed, 1.25f)
                  && Near(transform.position, Vector3{0.25f, 1.0f, 0.25f})
                  && Near(transform.yawRadians, 0.5f)
                  && Near(transform.rotationXRadians, 0.0f)
                  && Near(transform.rotationZRadians, 0.0f),
          "NPC runtime loops semantic Idle with floor transform, scale, and shadow settings");
}

void TestNpcNavigationRoutesIndependentAgentsAroundStaticCollision()
{
    game::SectorTopologyMap map = MakeNavigationSquareMap();
    game::SectorPlacedRuntimeObject obstacleObject;
    obstacleObject.id = 900;
    obstacleObject.kind = "static_model";
    obstacleObject.staticModel.collision = true;
    obstacleObject.staticModel.geometryFingerprint = "npc-navigation-test-obstacle";
    map.runtimeObjects.push_back(obstacleObject);
    game::SectorStaticModelCollider obstacle;
    obstacle.placedObjectId = 900;
    obstacle.center = {8.0f, 8.0f};
    obstacle.axisX = {1.0f, 0.0f};
    obstacle.axisZ = {0.0f, 1.0f};
    obstacle.halfExtents = {1.0f, 4.0f};
    obstacle.bottom = 0.0f;
    obstacle.top = 2.0f;
    obstacle.resolved = true;
    const std::vector<game::SectorStaticModelCollider> colliders{obstacle};

    game::SectorCollisionWorld collisionWorld;
    std::string collisionError;
    Check(collisionWorld.BuildFromTopology(map, &collisionError),
          "NPC navigation collision fixture builds");
    game::SectorNavigationWorld navigation;
    navigation.Initialize();
    navigation.RequestRebuild();
    FinishNavigationBuild(navigation, map, colliders);
    Check(navigation.State() == game::SectorNavigationState::Ready,
          "NPC static-obstacle navigation fixture builds");

    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 4);
    const engine::Entity walker = SpawnNavigationTestNpc(
            world, "walker", 101, {2.0f, 0.0f, 6.0f}, 10, 2.0f, 4.0f);
    const engine::Entity runner = SpawnNavigationTestNpc(
            world, "runner", 102, {2.0f, 0.0f, 10.0f}, 10, 2.0f, 4.0f);
    game::NpcNavigationRuntime npcNavigation;
    game::InitializeNpcNavigationRuntime(world, navigation, npcNavigation);
    Check(npcNavigation.records.size() == 2
                  && !(npcNavigation.records[0].agentHandle
                          == npcNavigation.records[1].agentHandle),
          "two NPCs own independent external navigation records");
    const auto walkRequest = game::RequestNpcMove(
            world, navigation, collisionWorld, npcNavigation,
            "walker", {14.0f, 6.0f}, game::NpcMoveGait::Walk);
    const auto runRequest = game::RequestNpcMove(
            world, navigation, collisionWorld, npcNavigation,
            "runner", {14.0f, 10.0f}, game::NpcMoveGait::Run);
    Check(walkRequest.accepted && runRequest.accepted,
          "programmatic Slice 3 seam accepts two independent move requests");
    Check(!(npcNavigation.records[0].pathHandle
                  == npcNavigation.records[1].pathHandle),
          "agents do not share mutable path records");
    for (game::NpcNavigationRecord& record : npcNavigation.records) {
        if (record.instanceId != "walker") continue;
        record.corners[0] = record.projectedDestination;
        record.cornerCount = 1;
        record.nextCorner = 0;
    }

    engine::AssetManager assets;
    game::NpcDefinitionCatalog definitions;
    game::SectorBakedObjectLightProbeRuntimeData probes;
    const std::vector<game::SectorDynamicDoorCollider> doors;
    bool observedWalk = false;
    bool observedRun = false;
    bool enteredObstacle = false;
    for (int frame = 0; frame < 800; ++frame) {
        const float dt = frame % 2 == 0 ? 0.016f : 0.08f;
        game::UpdateNpcNavigationAndLocomotionSystem(
                world, assets, navigation, npcNavigation, definitions,
                collisionWorld, doors, colliders, probes, map, dt);
        observedWalk = observedWalk
                || world.Get<game::NpcRuntimeInstance>(walker).action
                        == game::NpcAction::Walk;
        observedRun = observedRun
                || world.Get<game::NpcRuntimeInstance>(runner).action
                        == game::NpcAction::Run;
        const game::SectorObjectTransform& currentWalker =
                world.Get<game::SectorObjectTransform>(walker);
        enteredObstacle = enteredObstacle
                || (currentWalker.position.x > 7.0f
                    && currentWalker.position.x < 9.0f
                    && currentWalker.position.z > 4.0f
                    && currentWalker.position.z < 12.0f);
        if (game::GetNpcMoveStatus(npcNavigation, "walker").phase
                        == game::NpcMovePhase::Arrived
                && game::GetNpcMoveStatus(npcNavigation, "runner").phase
                        == game::NpcMovePhase::Arrived) {
            break;
        }
    }
    const game::NpcMoveStatus walkerStatus =
            game::GetNpcMoveStatus(npcNavigation, "walker");
    const game::NpcMoveStatus runnerStatus =
            game::GetNpcMoveStatus(npcNavigation, "runner");
    const game::SectorObjectTransform& walkerTransform =
            world.Get<game::SectorObjectTransform>(walker);
    const game::SectorObjectTransform& runnerTransform =
            world.Get<game::SectorObjectTransform>(runner);
    Check(walkerStatus.phase == game::NpcMovePhase::Arrived
                  && runnerStatus.phase == game::NpcMovePhase::Arrived
                  && walkerStatus.replanCount > 0
                  && observedWalk && observedRun,
          "walk/run actions arrive after collision feedback repairs a forced invalid corridor");
    Check(Near(walkerTransform.position.x, 14.0f, 0.11f)
                  && Near(runnerTransform.position.x, 14.0f, 0.11f)
                  && !Near(walkerTransform.yawRadians, 0.35f, 0.01f)
                  && !Near(runnerTransform.yawRadians, 0.35f, 0.01f),
          "locomotion reaches its tolerance and rotates only after movement begins");
    Check(!enteredObstacle
                  && !(walkerTransform.position.x > 7.0f
                    && walkerTransform.position.x < 9.0f
                    && walkerTransform.position.z > 4.0f
                    && walkerTransform.position.z < 12.0f)
                  && !(runnerTransform.position.x > 7.0f
                    && runnerTransform.position.x < 9.0f
                    && runnerTransform.position.z > 4.0f
                    && runnerTransform.position.z < 12.0f),
          "authoritative prop collision remains respected at final transforms");

    Check(game::RequestNpcMove(
                  world, navigation, collisionWorld, npcNavigation,
                  "walker", {2.0f, 6.0f}, game::NpcMoveGait::Walk).accepted
                  && !game::RequestNpcMove(
                          world, navigation, collisionWorld, npcNavigation,
                          "walker", {3.0f, 6.0f}, game::NpcMoveGait::Walk).accepted,
          "active move requests are not silently replaced");
    Check(game::CancelNpcMove(world, navigation, npcNavigation, "walker")
                  && game::GetNpcMoveStatus(npcNavigation, "walker").phase
                          == game::NpcMovePhase::Cancelled
                  && world.Get<game::NpcRuntimeInstance>(walker).action
                          == game::NpcAction::Idle,
          "programmatic cancellation releases the path and restores semantic Idle");

    const game::NpcMoveRequestResult aiRequest = game::RequestNpcMove(
            world,
            navigation,
            collisionWorld,
            npcNavigation,
            "walker",
            {3.0f, 6.0f},
            game::NpcMoveGait::Walk,
            game::NpcMoveAuthority::Ai);
    const game::NpcMoveRequestResult scriptTakeover = game::RequestNpcMove(
            world,
            navigation,
            collisionWorld,
            npcNavigation,
            "walker",
            {4.0f, 6.0f},
            game::NpcMoveGait::Run,
            game::NpcMoveAuthority::Script);
    const game::NpcMoveRequestResult blockedAi = game::RequestNpcMove(
            world,
            navigation,
            collisionWorld,
            npcNavigation,
            "walker",
            {5.0f, 6.0f},
            game::NpcMoveGait::Walk,
            game::NpcMoveAuthority::Ai);
    const game::NpcMoveStatus scriptStatus = game::GetNpcMoveStatus(
            npcNavigation, "walker");
    Check(aiRequest.accepted
                  && scriptTakeover.accepted
                  && scriptTakeover.requestId != aiRequest.requestId
                  && !blockedAi.accepted
                  && scriptStatus.authority == game::NpcMoveAuthority::Script
                  && scriptStatus.requestId == scriptTakeover.requestId,
          "script authority replaces AI intent and blocks AI retargeting");
    Check(!game::CancelNpcMove(
                  world,
                  navigation,
                  npcNavigation,
                  "walker",
                  aiRequest.requestId)
                  && game::CancelNpcMove(
                          world,
                          navigation,
                          npcNavigation,
                          "walker",
                          scriptTakeover.requestId),
          "request-specific cancellation cannot stop a replacement move");

    world.DestroyLater(walker);
    world.FlushDestroyedEntities();
    game::UpdateNpcNavigationAndLocomotionSystem(
            world, assets, navigation, npcNavigation, definitions,
            collisionWorld, doors, colliders, probes, map, 0.016f);
    Check(npcNavigation.records[0].occupied != npcNavigation.records[1].occupied,
          "deleted NPC releases only its own external navigation record");
    game::ShutdownNpcNavigationRuntime(world, navigation, npcNavigation);
}

void TestNpcNavigationLowSpeedAdvancesAtHighRefreshRate()
{
    game::SectorTopologyMap map = MakeNavigationSquareMap();
    game::SectorCollisionWorld collisionWorld;
    std::string collisionError;
    Check(collisionWorld.BuildFromTopology(map, &collisionError),
          "low-speed NPC collision fixture builds");

    game::SectorNavigationWorld navigation;
    navigation.Initialize();
    navigation.RequestRebuild();
    FinishNavigationBuild(navigation, map);
    Check(navigation.State() == game::SectorNavigationState::Ready,
          "low-speed NPC navigation fixture builds");

    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);
    const engine::Entity npcEntity = SpawnNavigationTestNpc(
            world, "slow_walker", 103, {3.0f, 0.0f, 8.0f}, 10, 1.0f, 3.0f);
    game::SectorObjectTransform& initialTransform =
            world.Get<game::SectorObjectTransform>(npcEntity);
    initialTransform.yawRadians = PI * 0.5f;

    game::NpcNavigationRuntime npcNavigation;
    game::InitializeNpcNavigationRuntime(world, navigation, npcNavigation);
    const game::NpcMoveRequestResult request = game::RequestNpcMove(
            world,
            navigation,
            collisionWorld,
            npcNavigation,
            "slow_walker",
            {2.0f, 8.0f},
            game::NpcMoveGait::Walk);
    Check(request.accepted,
          "one-unit-per-second NPC move is accepted");

    engine::AssetManager assets;
    game::NpcDefinitionCatalog definitions;
    game::SectorBakedObjectLightProbeRuntimeData probes;
    const std::vector<game::SectorDynamicDoorCollider> doors;
    const std::vector<game::SectorStaticModelCollider> colliders;
    constexpr float HighRefreshDt = 1.0f / 240.0f;
    game::UpdateNpcNavigationAndLocomotionSystem(
            world,
            assets,
            navigation,
            npcNavigation,
            definitions,
            collisionWorld,
            doors,
            colliders,
            probes,
            map,
            HighRefreshDt);

    const game::SectorObjectTransform& firstTransform =
            world.Get<game::SectorObjectTransform>(npcEntity);
    const game::NpcMoveStatus firstStatus = game::GetNpcMoveStatus(
            npcNavigation, "slow_walker");
    const float maximumTurn = 4.0f * PI * HighRefreshDt;
    Check(firstTransform.position.x < 3.0f
                  && Near(
                          firstTransform.position.x,
                          3.0f - HighRefreshDt,
                          0.0001f)
                  && firstStatus.actualVelocity.x < -0.99f
                  && world.Get<game::NpcRuntimeInstance>(npcEntity).action
                          == game::NpcAction::Walk,
          "one-unit-per-second NPC advances every 240 Hz frame");
    Check(firstTransform.yawRadians < PI * 0.5f
                  && firstTransform.yawRadians
                          >= PI * 0.5f - maximumTurn - 0.0001f
                  && std::fabs(firstTransform.yawRadians + PI * 0.5f) > 1.0f,
          "180-degree NPC turn is rate-limited instead of snapping");

    for (int frame = 0; frame < 400
            && game::GetNpcMoveStatus(npcNavigation, "slow_walker").phase
                    == game::NpcMovePhase::FollowingPath;
            ++frame) {
        game::UpdateNpcNavigationAndLocomotionSystem(
                world,
                assets,
                navigation,
                npcNavigation,
                definitions,
                collisionWorld,
                doors,
                colliders,
                probes,
                map,
                HighRefreshDt);
    }
    const game::NpcMoveStatus completed = game::GetNpcMoveStatus(
            npcNavigation, "slow_walker");
    Check(completed.phase == game::NpcMovePhase::Arrived
                  && completed.replanCount == 0
                  && Near(
                          world.Get<game::SectorObjectTransform>(npcEntity)
                                  .position.x,
                          2.0f,
                          0.11f),
          "one-unit-per-second NPC arrives without false stalls at 240 Hz");

    game::ShutdownNpcNavigationRuntime(world, navigation, npcNavigation);
    navigation.Shutdown();
}

void TestNpcSemanticAnimationUsesBlendingAndQueuesTransitions()
{
    game::NpcAnimationState state;
    state.resolved = true;
    state.animationIndices = {0, 1, 2};
    state.animationSpeeds = {0.8f, 1.25f, 1.6f};
    state.blendSeconds = 0.35f;
    state.appliedAction = game::NpcAction::Idle;
    engine::AnimatedModelAnimator animator;
    animator.animationIndex = 0;
    animator.speed = 0.8f;

    Check(game::ApplyNpcSemanticAnimation(
                  state, animator, game::NpcAction::Walk)
                    == game::NpcAnimationApplyResult::Applied
                  && animator.animationIndex == 0
                  && animator.targetAnimationIndex == 1
                  && Near(animator.transitionDurationSeconds, 0.35f)
                  && Near(animator.speed, 1.25f),
          "NPC Idle-to-Walk uses the authored blend time and semantic speed multiplier");
    Check(game::ApplyNpcSemanticAnimation(
                  state, animator, game::NpcAction::Run)
                    == game::NpcAnimationApplyResult::Queued
                  && state.hasPendingAction
                  && state.pendingAction == game::NpcAction::Run
                  && animator.targetAnimationIndex == 1,
          "NPC action changes queue while the current animation blend completes");
    animator.animationIndex = animator.targetAnimationIndex;
    animator.targetAnimationIndex = engine::InvalidModelAnimationIndex;
    Check(game::ApplyNpcSemanticAnimation(
                  state, animator, game::NpcAction::Run)
                    == game::NpcAnimationApplyResult::Applied
                  && animator.animationIndex == 1
                  && animator.targetAnimationIndex == 2
                  && Near(animator.transitionDurationSeconds, 0.35f)
                  && Near(animator.speed, 1.6f),
          "queued Run action begins a second blend after Idle-to-Walk completes");

    animator.animationIndex = animator.targetAnimationIndex;
    animator.targetAnimationIndex = engine::InvalidModelAnimationIndex;
    state.appliedAction = game::NpcAction::Run;
    state.animationIndices[static_cast<size_t>(game::NpcAction::Walk)] =
            engine::InvalidModelAnimationIndex;
    Check(game::ApplyNpcSemanticAnimation(
                  state, animator, game::NpcAction::Walk)
                    == game::NpcAnimationApplyResult::Missing
                  && state.appliedAction == game::NpcAction::Run,
          "missing semantic animation leaves the current animation valid and reports gracefully");
}

void TestNpcNavigationSmoothsSectorGeometryStairsVisually()
{
    const game::SectorTopologyMap map = MakeNavigationStairMap();
    game::SectorCollisionWorld collisionWorld;
    std::string collisionError;
    Check(collisionWorld.BuildFromTopology(map, &collisionError),
          "sector stair collision fixture builds");
    game::SectorNavigationWorld navigation;
    navigation.Initialize();
    navigation.RequestRebuild();
    FinishNavigationBuild(navigation, map);
    Check(navigation.State() == game::SectorNavigationState::Ready,
          "sector stair navigation fixture builds");

    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 2);
    const engine::Entity npcEntity = SpawnNavigationTestNpc(
            world, "stair_npc", 201, {0.5f, 0.0f, 2.0f}, 1, 1.5f, 3.0f);
    game::NpcNavigationRuntime npcNavigation;
    game::InitializeNpcNavigationRuntime(world, navigation, npcNavigation);
    Check(game::RequestNpcMove(
                  world, navigation, collisionWorld, npcNavigation,
                  "stair_npc", {5.5f, 2.0f}, game::NpcMoveGait::Walk).accepted,
          "NPC accepts an ascending sector-stair request");

    engine::AssetManager assets;
    game::NpcDefinitionCatalog definitions;
    game::SectorBakedObjectLightProbeRuntimeData probes;
    const std::vector<game::SectorDynamicDoorCollider> doors;
    const std::vector<game::SectorStaticModelCollider> colliders;
    bool observedAscendingVisualLag = false;
    for (int frame = 0; frame < 500; ++frame) {
        game::UpdateNpcNavigationAndLocomotionSystem(
                world, assets, navigation, npcNavigation, definitions,
                collisionWorld, doors, colliders, probes, map, 0.04f);
        const game::SectorObjectTransform& transform =
                world.Get<game::SectorObjectTransform>(npcEntity);
        const game::SectorObjectVisualOffset& offset =
                world.Get<game::SectorObjectVisualOffset>(npcEntity);
        observedAscendingVisualLag = observedAscendingVisualLag
                || (transform.position.y > 0.0f && offset.position.y < -0.001f);
        if (game::GetNpcMoveStatus(npcNavigation, "stair_npc").phase
                == game::NpcMovePhase::Arrived) {
            break;
        }
    }
    const game::SectorObjectTransform& topTransform =
            world.Get<game::SectorObjectTransform>(npcEntity);
    Check(game::GetNpcMoveStatus(npcNavigation, "stair_npc").phase
                    == game::NpcMovePhase::Arrived
                  && Near(topTransform.position.y, 1.0f, 0.001f)
                  && world.Get<game::SectorObject>(npcEntity).currentSectorId == 6,
          "NPC physically snaps through successive valid steps and reaches the top sector");
    Check(observedAscendingVisualLag,
          "ascending physical step snaps are hidden by a separate presentation offset");

    Check(game::RequestNpcMove(
                  world, navigation, collisionWorld, npcNavigation,
                  "stair_npc", {0.5f, 2.0f}, game::NpcMoveGait::Walk).accepted,
          "NPC accepts a descending sector-stair request");
    bool observedDescendingVisualLag = false;
    for (int frame = 0; frame < 500; ++frame) {
        game::UpdateNpcNavigationAndLocomotionSystem(
                world, assets, navigation, npcNavigation, definitions,
                collisionWorld, doors, colliders, probes, map, 0.04f);
        const game::SectorObjectVisualOffset& offset =
                world.Get<game::SectorObjectVisualOffset>(npcEntity);
        observedDescendingVisualLag = observedDescendingVisualLag
                || offset.position.y > 0.001f;
        if (game::GetNpcMoveStatus(npcNavigation, "stair_npc").phase
                == game::NpcMovePhase::Arrived) {
            break;
        }
    }
    for (int frame = 0; frame < 30; ++frame) {
        game::UpdateNpcNavigationAndLocomotionSystem(
                world, assets, navigation, npcNavigation, definitions,
                collisionWorld, doors, colliders, probes, map, 0.04f);
    }
    const game::SectorObjectTransform& bottomTransform =
            world.Get<game::SectorObjectTransform>(npcEntity);
    const game::SectorObjectVisualOffset& settledOffset =
            world.Get<game::SectorObjectVisualOffset>(npcEntity);
    Check(game::GetNpcMoveStatus(npcNavigation, "stair_npc").phase
                    == game::NpcMovePhase::Arrived
                  && Near(bottomTransform.position.y, 0.0f, 0.001f)
                  && world.Get<game::SectorObject>(npcEntity).currentSectorId == 1
                  && observedDescendingVisualLag,
          "NPC descends the same sector staircase with smoothed visual step-downs");
    Check(std::fabs(settledOffset.position.y) < 0.001f,
          "presentation-only stair offset decays without feeding physics or sector lookup");
    Check(game::RequestNpcMove(
                  world, navigation, collisionWorld, npcNavigation,
                  "stair_npc", {5.5f, 2.0f}, game::NpcMoveGait::Run).accepted,
          "NPC can begin a fresh move after arrival");
    game::UpdateNpcNavigationAndLocomotionSystem(
            world, assets, navigation, npcNavigation, definitions,
            collisionWorld, doors, colliders, probes, map, 0.04f);
    world.Get<game::SectorObjectVisualOffset>(npcEntity).position.y = 0.1f;
    game::ShutdownNpcNavigationRuntime(world, navigation, npcNavigation);
    Check(npcNavigation.records.empty()
                  && world.Get<game::NpcRuntimeInstance>(npcEntity).action
                          == game::NpcAction::Idle
                  && Near(world.Get<game::SectorObjectVisualOffset>(npcEntity).position.y, 0.0f),
          "map teardown cancels locomotion, restores Idle, and clears visual smoothing state");
}

void TestSpawnNpcMissingDefinitionRemainsDiagnosticSkip()
{
    engine::World world;
    engine::AssetManager assets;
    game::SectorRuntimeObjectState state;
    game::SectorTopologyMap map = MakeSquareMap();
    game::SectorPlacedRuntimeObject object;
    object.id = 32;
    object.kind = "npc";
    object.position = Vector3{2.0f, 0.0f, 2.0f};
    object.npc.definitionId = "definitely_missing_test_npc";
    map.runtimeObjects.push_back(object);

    game::RefreshSectorRuntimeObjectMapData(state, map);
    state.npcDefinitionCatalog = game::NpcDefinitionCatalog{};
    game::SpawnPlacedRuntimeObjects(world, assets, state, map);
    Check(state.placedObjectEntities.empty()
                  && state.spawnedObjectCount == 0
                  && state.skippedObjectCount == 1
                  && state.placedObjectWarning.find("was not found")
                          != std::string::npos,
          "missing NPC definition skips only the runtime entity and reports a diagnostic");
}

void TestAnimatedModelSelectionAndBlendApi()
{
    ModelAnimation animations[3] = {};
    std::strncpy(animations[0].name, "Idle", sizeof(animations[0].name) - 1);
    std::strncpy(animations[1].name, "Walk", sizeof(animations[1].name) - 1);
    std::strncpy(animations[2].name, "Open", sizeof(animations[2].name) - 1);
    engine::ModelAsset asset;
    asset.animations = animations;
    asset.animationCount = 3;
    Check(engine::FindModelAnimationIndex(asset, "Walk") == 1
                  && engine::FindModelAnimationIndex(asset, "Missing")
                          == engine::InvalidModelAnimationIndex,
          "animated model API resolves named clips without per-frame string lookup");

    engine::AnimatedModelAnimator animator;
    engine::SetAnimatedModelAnimation(animator, 0);
    animator.frame = 12.0f;
    engine::SetAnimatedModelAnimation(animator, 1, 0.4f);
    Check(animator.animationIndex == 0
                  && animator.targetAnimationIndex == 1
                  && Near(animator.frame, 12.0f)
                  && Near(animator.transitionDurationSeconds, 0.4f),
          "positive-duration animation selection preserves a source clip for blending");
    engine::SetAnimatedModelAnimation(animator, 2, 0.0f);
    Check(animator.animationIndex == 2
                  && animator.targetAnimationIndex == engine::InvalidModelAnimationIndex
                  && Near(animator.frame, 0.0f),
          "zero-duration animation selection switches immediately and restarts");
}

void TestStaticModelAuxiliaryMaterialMapsBindDrawMeshTextures()
{
    std::array<
            MaterialMap,
            game::SectorStaticModelMaterialMapCount> maps{};
    maps[MATERIAL_MAP_DIFFUSE].texture.id = 7;
    maps[game::SectorStaticModelLightmapMaterialMap].texture.id = 70;
    maps[game::SectorStaticModelShadowMap0MaterialMap].texture.id = 80;
    maps[game::SectorStaticModelShadowMap1MaterialMap].texture.id = 90;
    maps[game::SectorStaticModelEnvironmentMaterialMap].texture.id = 75;
    const Texture2D lightmap{101, 2048, 2048, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    const Texture2D shadowMap0{102, 1024, 1024, 1, PIXELFORMAT_UNCOMPRESSED_R32};
    const Texture2D shadowMap1{103, 1024, 1024, 1, PIXELFORMAT_UNCOMPRESSED_R32};
    const TextureCubemap environment{104, 256, 256, 9, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

    game::ConfigureSectorStaticModelAuxiliaryMaterialMaps(
            maps,
            &lightmap,
            true,
            &environment,
            &shadowMap0,
            &shadowMap1);
    Check(maps[MATERIAL_MAP_DIFFUSE].texture.id == 7
                  && maps[game::SectorStaticModelLightmapMaterialMap]
                             .texture.id
                          == lightmap.id
                  && maps[game::SectorStaticModelShadowMap0MaterialMap]
                             .texture.id
                          == shadowMap0.id
                  && maps[game::SectorStaticModelShadowMap1MaterialMap]
                             .texture.id
                          == shadowMap1.id
                  && maps[game::SectorStaticModelEnvironmentMaterialMap]
                             .texture.id
                          == environment.id,
          "static prop DrawMesh material maps bind PBR environment, lightmap, and spotlight shadow textures without replacing albedo");

    game::ConfigureSectorStaticModelAuxiliaryMaterialMaps(
            maps,
            &lightmap,
            false,
            nullptr,
            nullptr,
            nullptr);
    Check(maps[game::SectorStaticModelLightmapMaterialMap]
                          .texture.id
                  == 0
                  && maps[game::SectorStaticModelShadowMap0MaterialMap]
                             .texture.id
                          == 0
                  && maps[game::SectorStaticModelShadowMap1MaterialMap]
                             .texture.id
                          == 0
                  && maps[game::SectorStaticModelEnvironmentMaterialMap]
                             .texture.id
                          == 0,
          "static prop fallback clears auxiliary material textures when no prop lightmap is valid");
}

void TestStaticModelSpotlightShadowCasterCollectionAndRevision()
{
    engine::World world;
    game::ReserveSectorRuntimeObjectWorld(world, 8);

    const engine::Entity visible = world.CreateEntity();
    const game::SectorObjectTransform visibleTransform{
            Vector3{2.0f, 3.0f, -4.0f},
            0.35f,
            -0.2f,
            0.1f};
    world.Add(visible, visibleTransform);
    world.Add(visible, game::SectorObject{7, true});
    world.Add(visible, game::SectorStaticModel{
            engine::ModelHandle{3, 5},
            41,
            Vector3{0.1f, 0.1f, 0.1f},
            1.75f,
            0.2f});

    const engine::Entity hidden = world.CreateEntity();
    world.Add(hidden, game::SectorObjectTransform{});
    world.Add(hidden, game::SectorObject{7, false});
    world.Add(hidden, game::SectorStaticModel{
            engine::ModelHandle{4, 2},
            42});

    const engine::Entity unassigned = world.CreateEntity();
    world.Add(unassigned, game::SectorObjectTransform{});
    world.Add(unassigned, game::SectorObject{7, true});
    world.Add(unassigned, game::SectorStaticModel{});

    game::SectorStaticModelShadowCasterCollection collection;
    game::ReserveSectorStaticModelShadowCasters(collection, 8);
    game::UpdateSectorStaticModelShadowCasters(collection, &world);

    Check(collection.casters.size() == 1,
          "spotlight shadow collection includes only visible assigned static props");
    const game::SectorStaticModelShadowCaster& caster =
            collection.casters.front();
    const Matrix expected = game::BuildSectorStaticModelAuthoredTransform(
            visibleTransform.position,
            visibleTransform.rotationXRadians,
            visibleTransform.yawRadians,
            visibleTransform.rotationZRadians,
            1.75f);
    Check(caster.placedObjectId == 41
                  && caster.model == engine::ModelHandle{3, 5}
                  && Near(
                             Vector3Transform(Vector3{}, caster.transform),
                             Vector3Transform(Vector3{}, expected))
                  && Near(
                             Vector3Transform(Vector3{1.0f, 0.0f, 0.0f}, caster.transform),
                             Vector3Transform(Vector3{1.0f, 0.0f, 0.0f}, expected))
                  && Near(
                             Vector3Transform(Vector3{0.0f, 1.0f, 0.0f}, caster.transform),
                             Vector3Transform(Vector3{0.0f, 1.0f, 0.0f}, expected)),
          "static prop spotlight caster uses the visible renderer authored transform");

    const uint64_t initialRevision = collection.revision;
    game::UpdateSectorStaticModelShadowCasters(collection, &world);
    Check(collection.revision == initialRevision,
          "unchanged static prop shadow casters preserve the cache revision");

    world.Get<game::SectorObjectTransform>(visible).position.x += 0.5f;
    game::UpdateSectorStaticModelShadowCasters(collection, &world);
    const uint64_t movedRevision = collection.revision;
    Check(movedRevision != initialRevision,
          "moving a static prop invalidates the spotlight shadow caster revision");

    world.Get<game::SectorObject>(visible).visible = false;
    game::UpdateSectorStaticModelShadowCasters(collection, &world);
    const uint64_t hiddenRevision = collection.revision;
    Check(collection.casters.empty()
                  && hiddenRevision != movedRevision,
          "hiding the final static prop empties casters and invalidates shadows");
    game::UpdateSectorStaticModelShadowCasters(collection, &world);
    Check(collection.revision == hiddenRevision,
          "an unchanged empty static prop caster set keeps its revision stable");
}

void TestViewmodelMaterialOverrideResolution()
{
    float metallicFactor = 0.5f;
    float roughnessFactor = 0.22f;
    bool hasMetallicTexture = true;
    bool hasRoughnessTexture = true;
    game::SectorViewmodelLightingContext unchanged;
    Check(Near(unchanged.brightnessMultiplier, 1.0f),
          "viewmodel lighting context defaults to neutral output brightness");
    game::ApplySectorViewmodelMaterialOverride(
            unchanged,
            metallicFactor,
            roughnessFactor,
            hasMetallicTexture,
            hasRoughnessTexture);
    Check(Near(metallicFactor, 0.5f)
                  && Near(roughnessFactor, 0.22f)
                  && hasMetallicTexture
                  && hasRoughnessTexture,
          "disabled viewmodel material override preserves imported material values");

    game::SectorViewmodelLightingContext corrected;
    corrected.materialOverrideEnabled = true;
    corrected.metallicFactor = 0.0f;
    corrected.roughnessFactor = 0.78f;
    corrected.useMetallicRoughnessTexture = false;
    game::ApplySectorViewmodelMaterialOverride(
            corrected,
            metallicFactor,
            roughnessFactor,
            hasMetallicTexture,
            hasRoughnessTexture);
    Check(Near(metallicFactor, 0.0f)
                  && Near(roughnessFactor, 0.78f)
                  && !hasMetallicTexture
                  && !hasRoughnessTexture,
          "viewmodel material override replaces factors and disables the packed texture");
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
    world.Add(object, game::SectorObjectTransform{Vector3{2.0f, 1.05f, 2.0f}, 0.0f});
    world.Add(object, game::SectorObject{10, true});
    world.Add(object, game::SectorObjectLighting{});

    game::UpdateSectorObjectBakedLightingSystem(world, MakeProbeRuntimeData(), nullptr);

    const game::SectorObjectLighting& lighting = world.Get<game::SectorObjectLighting>(object);
    Check(lighting.baked.valid,
            "runtime object baked lighting system stores valid probe sample");
    Check(Near(lighting.baked.ambientCube[0], Vector3{0.5f, 0.3f, 0.5f}),
            "runtime object baked lighting system resolves its root-height ambient cube");
    Check(Near(lighting.vertical.lower.ambientCube[0], Vector3{0.8f, 0.25f, 0.1f})
                  && Near(lighting.vertical.upper.ambientCube[0], Vector3{0.2f, 0.35f, 0.9f})
                  && Near(lighting.vertical.lowerHeightWorld, 0.6f)
                  && Near(lighting.vertical.upperHeightWorld, 1.5f),
            "runtime object baked lighting system retains both layers for per-fragment rendering");
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
    Check(Near(lighting.vertical.lower.ambientCube[0], lighting.vertical.upper.ambientCube[0]),
            "runtime object baked lighting fallback duplicates a safe vertical sample");
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

game::SectorStaticModelCollider MakeStaticModelCollider(
        Vector2 center,
        Vector2 halfExtents,
        float bottom,
        float top)
{
    game::SectorStaticModelCollider collider;
    collider.placedObjectId = 1;
    collider.center = center;
    collider.axisX = Vector2{1.0f, 0.0f};
    collider.axisZ = Vector2{0.0f, 1.0f};
    collider.halfExtents = halfExtents;
    collider.bottom = bottom;
    collider.top = top;
    collider.resolved = true;
    return collider;
}

void TestPropAndDoorStandingClearanceQueries()
{
    const Vector2 playerPosition{};
    const float feetY = 0.0f;
    const float radius = 0.25f;
    const std::vector<game::SectorStaticModelCollider> overheadProps{
            MakeStaticModelCollider(
                    Vector2{},
                    Vector2{1.0f, 1.0f},
                    1.1f,
                    2.0f)};
    Check(game::SectorStaticModelCollidersAllowPlayerHeight(
                  playerPosition,
                  feetY,
                  radius,
                  1.0f,
                  overheadProps),
          "crouched player height fits beneath a generated prop underside");
    Check(!game::SectorStaticModelCollidersAllowPlayerHeight(
                  playerPosition,
                  feetY,
                  radius,
                  1.6f,
                  overheadProps),
          "standing player height is rejected beneath a generated prop underside");
    Check(game::SectorStaticModelCollidersAllowPlayerHeight(
                  Vector2{3.0f, 0.0f},
                  feetY,
                  radius,
                  1.6f,
                  overheadProps),
          "prop standing clearance ignores horizontally separate colliders");

    const std::vector<game::SectorDynamicDoorCollider> raisedDoors{
            MakeDynamicDoorCollider(
                    Vector2{},
                    Vector2{1.0f, 0.125f},
                    1.1f,
                    2.0f)};
    Check(game::SectorDoorDynamicCollidersAllowPlayerHeight(
                  playerPosition,
                  feetY,
                  radius,
                  1.0f,
                  raisedDoors),
          "crouched player height fits beneath a generated raised door");
    Check(!game::SectorDoorDynamicCollidersAllowPlayerHeight(
                  playerPosition,
                  feetY,
                  radius,
                  1.6f,
                  raisedDoors),
          "standing player height is rejected beneath a generated raised door");

    const std::vector<game::SectorStaticModelCollider> supportingProps{
            MakeStaticModelCollider(
                    Vector2{},
                    Vector2{1.0f, 1.0f},
                    -1.0f,
                    0.0f)};
    Check(game::SectorStaticModelCollidersAllowPlayerHeight(
                  playerPosition,
                  feetY,
                  radius,
                  1.6f,
                  supportingProps),
          "prop supporting the player's feet is not mistaken for overhead clearance");
}

game::SectorCollisionMoveResult ResolveStaticModelMovement(
        const game::SectorFpsControllerState& state,
        Vector2 destination,
        const game::SectorFpsControllerConfig& fpsConfig,
        const std::vector<game::SectorStaticModelCollider>& colliders,
        game::SectorFpsVerticalContext sectorContext = {true, 0.0f, 4.0f})
{
    const game::SectorFpsControllerConfig config =
            game::NormalizeSectorFpsControllerConfig(fpsConfig);
    return game::ResolveSectorStaticModelCollidersForPlayerMovement(
            game::SectorCollisionMoveState{
                    Vector2{state.feetPosition.x, state.feetPosition.z},
                    state.feetPosition.y,
                    state.currentSectorId,
                    state.grounded},
            game::SectorCollisionMoveResult{destination, state.currentSectorId},
            game::SectorCollisionMoveConfig{
                    config.playerRadius,
                    config.playerHeight,
                    config.stepHeight,
                    4},
            sectorContext,
            colliders);
}

void TestStaticModelColliderBuildUsesFullAuthoredTransform()
{
    const BoundingBox bounds{
            Vector3{-1.0f, -0.5f, -2.0f},
            Vector3{3.0f, 1.5f, 2.0f}};
    const game::SectorObjectTransform transform{
            Vector3{10.0f, 2.0f, 20.0f},
            PI * 0.5f};
    game::SectorStaticModelCollider collider;
    Check(game::BuildSectorStaticModelCollider(
                  7,
                  bounds,
                  transform,
                  2.0f,
                  collider),
          "static model bounds build a valid oriented collider");

    const Matrix authoredTransform = MatrixMultiply(
            MatrixScale(2.0f, 2.0f, 2.0f),
            MatrixMultiply(
                    MatrixRotateY(transform.yawRadians),
                    MatrixTranslate(10.0f, 2.0f, 20.0f)));
    const Vector3 expectedCenter = Vector3Transform(
            Vector3{1.0f, 0.5f, 0.0f},
            authoredTransform);
    Check(collider.placedObjectId == 7
                  && collider.resolved
                  && !collider.failed
                  && Near(collider.center, Vector2{expectedCenter.x, expectedCenter.z})
                  && Near(collider.halfExtents, Vector2{4.0f, 4.0f})
                  && Near(collider.bottom, 1.0f)
                  && Near(collider.top, 5.0f),
          "static model collider applies off-center bounds, scale, position, and height");
    Check(Near(collider.axisX.x * collider.axisZ.x
                       + collider.axisX.y * collider.axisZ.y,
               0.0f)
                  && Near(Vector2Length(collider.axisX), 1.0f)
                  && Near(Vector2Length(collider.axisZ), 1.0f),
          "static model collider yaw axes remain orthonormal");
}

void TestStaticModelXzRotationUsesSharedTransformAndEnclosingBounds()
{
    const Vector3 position{4.0f, 3.0f, 6.0f};
    const float yaw = 0.65f;
    const Matrix yawOnly = game::BuildSectorStaticModelAuthoredTransform(
            position,
            0.0f,
            yaw,
            0.0f,
            1.5f);
    const Matrix legacyYawOnly = MatrixMultiply(
            MatrixScale(1.5f, 1.5f, 1.5f),
            MatrixMultiply(
                    MatrixRotateY(yaw),
                    MatrixTranslate(position.x, position.y, position.z)));
    const Vector3 sample{1.25f, -0.5f, 2.0f};
    Check(Near(
                  Vector3Transform(sample, yawOnly),
                  Vector3Transform(sample, legacyYawOnly)),
          "shared static model transform preserves legacy yaw-only placement");

    const Matrix xyz = game::BuildSectorStaticModelAuthoredTransform(
            position,
            0.4f,
            yaw,
            -0.3f,
            1.5f);
    const Matrix expectedXyz = MatrixMultiply(
            MatrixScale(1.5f, 1.5f, 1.5f),
            MatrixMultiply(
                    MatrixRotateXYZ(Vector3{0.4f, yaw, -0.3f}),
                    MatrixTranslate(position.x, position.y, position.z)));
    Check(Near(
                  Vector3Transform(sample, xyz),
                  Vector3Transform(sample, expectedXyz)),
          "shared static model transform composes local X, Y, and Z rotation");

    const BoundingBox bounds{
            Vector3{-1.0f, -0.5f, -2.0f},
            Vector3{1.0f, 0.5f, 2.0f}};
    const game::SectorObjectTransform tilted{
            position,
            0.0f,
            PI * 0.5f,
            0.0f};
    game::SectorStaticModelCollider collider;
    Check(game::BuildSectorStaticModelCollider(
                  8,
                  bounds,
                  tilted,
                  1.0f,
                  collider),
          "X-tilted static model bounds build a valid collider");
    Check(Near(collider.center, Vector2{position.x, position.z})
                  && Near(collider.halfExtents, Vector2{1.0f, 0.5f})
                  && Near(collider.bottom, 1.0f)
                  && Near(collider.top, 5.0f),
          "tilted collision conservatively encloses all transformed model bounds");
}

void TestStaticModelColliderBlocksSweepsAndSlides()
{
    const std::vector<game::SectorStaticModelCollider> colliders{
            MakeStaticModelCollider(
                    Vector2{2.0f, 0.0f},
                    Vector2{0.5f, 4.0f},
                    0.0f,
                    1.0f)};
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 0.0f, 0.0f};
    state.currentSectorId = 10;
    state.grounded = true;
    game::SectorFpsControllerConfig config;

    const game::SectorCollisionMoveResult blocked = ResolveStaticModelMovement(
            state,
            Vector2{5.0f, 0.0f},
            config,
            colliders);
    Check(blocked.hitWall && blocked.blockedByStep,
          "tall static model box blocks grounded movement as a step");
    Check(blocked.positionXZ.x < 1.26f,
          "swept static model collision prevents high-speed tunneling");

    const game::SectorCollisionMoveResult slide = ResolveStaticModelMovement(
            state,
            Vector2{5.0f, 1.0f},
            config,
            colliders);
    Check(slide.hitWall
                  && slide.positionXZ.x < 1.26f
                  && slide.positionXZ.y > 0.5f,
          "diagonal static model contact preserves tangential sliding");
}

void TestStaticModelColliderContactAllowsEscapeAndTangentMovement()
{
    const Vector2 halfExtents{0.46f, 0.30f};
    const std::vector<game::SectorStaticModelCollider> colliders{
            MakeStaticModelCollider(Vector2{}, halfExtents, 0.0f, 1.9f)};
    game::SectorFpsControllerConfig config;
    config.playerRadius = 0.5f;
    config.playerHeight = 1.75f;
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{
            -(halfExtents.x + config.playerRadius),
            0.5f,
            0.0f};
    state.currentSectorId = 10;
    state.grounded = false;

    const Vector2 contact{state.feetPosition.x, state.feetPosition.z};
    const game::SectorCollisionMoveResult away = ResolveStaticModelMovement(
            state,
            Vector2{contact.x - 0.4f, contact.y},
            config,
            colliders);
    Check(away.positionXZ.x < contact.x - 0.39f,
          "static model face contact permits separating movement");

    const game::SectorCollisionMoveResult tangent = ResolveStaticModelMovement(
            state,
            Vector2{contact.x, contact.y + 0.4f},
            config,
            colliders);
    Check(Near(tangent.positionXZ, Vector2{contact.x, contact.y + 0.4f}),
          "static model face contact permits tangential movement");

    const game::SectorCollisionMoveResult inward = ResolveStaticModelMovement(
            state,
            Vector2{contact.x + 0.4f, contact.y},
            config,
            colliders);
    Check(inward.hitWall && inward.positionXZ.x <= contact.x + 0.001f,
          "static model face contact still blocks inward movement");
}

void TestStaticModelColliderRoundedCornerSweep()
{
    const std::vector<game::SectorStaticModelCollider> colliders{
            MakeStaticModelCollider(
                    Vector2{},
                    Vector2{0.5f, 0.5f},
                    0.0f,
                    2.0f)};
    game::SectorFpsControllerConfig config;
    config.playerRadius = 0.5f;
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{1.1f, 0.5f, 0.9f};
    state.currentSectorId = 10;

    const game::SectorCollisionMoveResult nearMiss = ResolveStaticModelMovement(
            state,
            Vector2{0.9f, 1.1f},
            config,
            colliders);
    Check(!nearMiss.hitWall && Near(nearMiss.positionXZ, Vector2{0.9f, 1.1f}),
          "rounded OBB corner does not block a circle path through empty corner space");

    state.feetPosition = Vector3{1.2f, 0.5f, 1.2f};
    const game::SectorCollisionMoveResult cornerHit = ResolveStaticModelMovement(
            state,
            Vector2{0.6f, 0.6f},
            config,
            colliders);
    const Vector2 fromCorner = Vector2Subtract(
            cornerHit.positionXZ,
            Vector2{0.5f, 0.5f});
    Check(cornerHit.hitWall
                  && Vector2Length(fromCorner) >= config.playerRadius - 0.001f,
          "true rounded-corner impact blocks with circle clearance");
}

void TestStaticModelColliderRotatedContactAndPenetrationRecovery()
{
    game::SectorStaticModelCollider collider = MakeStaticModelCollider(
            Vector2{},
            Vector2{0.46f, 0.30f},
            0.0f,
            1.9f);
    const float angle = PI * 0.25f;
    collider.axisX = Vector2{std::cos(angle), std::sin(angle)};
    collider.axisZ = Vector2{-std::sin(angle), std::cos(angle)};
    const std::vector<game::SectorStaticModelCollider> colliders{collider};
    game::SectorFpsControllerConfig config;
    config.playerRadius = 0.5f;
    config.playerHeight = 1.75f;
    const float contactDistance = collider.halfExtents.x + config.playerRadius;
    const Vector2 contact = Vector2Scale(collider.axisX, -contactDistance);
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{contact.x, 0.5f, contact.y};
    state.currentSectorId = 10;

    const Vector2 awayDestination = Vector2Add(
            contact,
            Vector2Scale(collider.axisX, -0.4f));
    const game::SectorCollisionMoveResult away = ResolveStaticModelMovement(
            state,
            awayDestination,
            config,
            colliders);
    Check(Near(away.positionXZ, awayDestination),
          "rotated static model contact permits movement away from its face");

    const Vector2 tangentDestination = Vector2Add(
            contact,
            Vector2Scale(collider.axisZ, 0.4f));
    const game::SectorCollisionMoveResult tangent = ResolveStaticModelMovement(
            state,
            tangentDestination,
            config,
            colliders);
    Check(Near(tangent.positionXZ, tangentDestination),
          "rotated static model contact preserves tangential movement");

    const Vector2 penetrating = Vector2Scale(
            collider.axisX,
            -(contactDistance - 0.1f));
    state.feetPosition = Vector3{penetrating.x, 0.5f, penetrating.y};
    const Vector2 intendedDelta = Vector2Scale(collider.axisX, -0.3f);
    const game::SectorCollisionMoveResult recovered = ResolveStaticModelMovement(
            state,
            Vector2Add(penetrating, intendedDelta),
            config,
            colliders);
    const float recoveredAlongAxis = Vector2DotProduct(
            recovered.positionXZ,
            collider.axisX);
    Check(recoveredAlongAxis <= -contactDistance - 0.29f,
          "starting penetration is recovered before intended escape movement is applied");
}

void TestStaticModelColliderRepeatedContactCanBackAway()
{
    const std::vector<game::SectorStaticModelCollider> colliders{
            MakeStaticModelCollider(
                    Vector2{},
                    Vector2{0.46f, 0.30f},
                    0.0f,
                    1.9f)};
    game::SectorFpsControllerConfig config;
    config.playerRadius = 0.5f;
    config.playerHeight = 1.75f;
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{-2.0f, 0.5f, 0.0f};
    state.currentSectorId = 10;

    const game::SectorCollisionMoveResult impact = ResolveStaticModelMovement(
            state,
            Vector2{0.0f, 0.0f},
            config,
            colliders);
    Check(impact.hitWall && impact.positionXZ.x < -0.95f,
          "synthetic tall prop blocks an aggressive inbound sweep");

    state.feetPosition.x = impact.positionXZ.x;
    state.feetPosition.z = impact.positionXZ.y;
    const game::SectorCollisionMoveResult escaped = ResolveStaticModelMovement(
            state,
            Vector2{impact.positionXZ.x - 0.5f, impact.positionXZ.y},
            config,
            colliders);
    Check(escaped.positionXZ.x < impact.positionXZ.x - 0.49f,
          "player can back away on the frame after aggressive prop contact");
}

void TestStaticModelColliderUsesVerticalOverlapAndStepHeight()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 0.0f, 0.0f};
    state.currentSectorId = 10;
    state.grounded = true;
    game::SectorFpsControllerConfig config;

    const std::vector<game::SectorStaticModelCollider> elevated{
            MakeStaticModelCollider(
                    Vector2{2.0f, 0.0f},
                    Vector2{0.5f, 0.5f},
                    2.0f,
                    3.0f)};
    Check(Near(ResolveStaticModelMovement(
                       state,
                       Vector2{2.0f, 0.0f},
                       config,
                       elevated)
                       .positionXZ,
               Vector2{2.0f, 0.0f}),
          "player passes below a static model box with vertical clearance");

    state.feetPosition.y = 1.0f;
    state.grounded = false;
    const std::vector<game::SectorStaticModelCollider> below{
            MakeStaticModelCollider(
                    Vector2{2.0f, 0.0f},
                    Vector2{0.5f, 0.5f},
                    0.0f,
                    1.0f)};
    Check(Near(ResolveStaticModelMovement(
                       state,
                       Vector2{2.0f, 0.0f},
                       config,
                       below)
                       .positionXZ,
               Vector2{2.0f, 0.0f}),
          "airborne player passes over a static model once feet reach its top");

    state.feetPosition.y = 0.0f;
    state.grounded = true;
    const std::vector<game::SectorStaticModelCollider> low{
            MakeStaticModelCollider(
                    Vector2{2.0f, 0.0f},
                    Vector2{0.5f, 0.5f},
                    0.0f,
                    0.2f)};
    const game::SectorCollisionMoveResult stepped = ResolveStaticModelMovement(
            state,
            Vector2{2.0f, 0.0f},
            config,
            low);
    Check(!stepped.hitWall && Near(stepped.positionXZ, Vector2{2.0f, 0.0f}),
          "static model top within Step Height allows horizontal entry");
    state.feetPosition.x = stepped.positionXZ.x;
    state.feetPosition.z = stepped.positionXZ.y;
    const game::SectorFpsVerticalContext steppedContext =
            game::BuildSectorStaticModelVerticalContext(
                    game::SectorFpsVerticalContext{true, 0.0f, 4.0f},
                    state,
                    config,
                    low);
    const game::SectorFpsVerticalResult steppedVertical =
            game::UpdateSectorFpsVerticalPhysics(
                    state,
                    config,
                    steppedContext,
                    0.0f);
    Check(steppedVertical.transition == game::SectorFpsVerticalTransition::SteppedUp
                  && Near(state.feetPosition.y, 0.2f),
          "low static model top becomes the grounded support surface");

    state.feetPosition = Vector3{0.0f, 0.0f, 0.0f};
    state.grounded = true;
    const game::SectorCollisionMoveResult noHeadroom = ResolveStaticModelMovement(
            state,
            Vector2{2.0f, 0.0f},
            config,
            low,
            game::SectorFpsVerticalContext{true, 0.0f, 1.7f});
    Check(noHeadroom.hitWall && noHeadroom.blockedByCeiling,
          "step-height static model still blocks when there is no player headroom");
}

void TestStaticModelVerticalContextSelectsNearestSurfaces()
{
    std::vector<game::SectorStaticModelCollider> colliders{
            MakeStaticModelCollider(
                    Vector2{},
                    Vector2{1.0f, 1.0f},
                    0.0f,
                    0.2f),
            MakeStaticModelCollider(
                    Vector2{},
                    Vector2{1.0f, 1.0f},
                    0.2f,
                    0.5f),
            MakeStaticModelCollider(
                    Vector2{},
                    Vector2{1.0f, 1.0f},
                    2.0f,
                    2.5f),
            MakeStaticModelCollider(
                    Vector2{},
                    Vector2{1.0f, 1.0f},
                    1.6f,
                    1.8f)};
    game::SectorFpsControllerState state;
    state.feetPosition.y = 1.0f;
    state.currentSectorId = 10;
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 0.5f;
    config.playerHeight = 0.5f;
    const game::SectorFpsVerticalContext context =
            game::BuildSectorStaticModelVerticalContext(
                    game::SectorFpsVerticalContext{true, 0.0f, 4.0f},
                    state,
                    config,
                    colliders);
    Check(Near(context.floorZ, 0.5f)
                  && Near(context.ceilingZ, 1.6f),
          "multiple static model boxes select the highest floor and nearest underside");
}

void TestStaticModelColliderLandingUndersideAndWalkOff()
{
    const std::vector<game::SectorStaticModelCollider> platform{
            MakeStaticModelCollider(
                    Vector2{0.0f, 0.0f},
                    Vector2{1.0f, 1.0f},
                    0.0f,
                    0.5f)};
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.0f;
    config.playerHeight = 1.0f;
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 1.0f, 0.0f};
    state.currentSectorId = 10;
    state.verticalVelocity = -2.0f;
    const game::SectorFpsVerticalContext landingContext =
            game::BuildSectorStaticModelVerticalContext(
                    game::SectorFpsVerticalContext{true, 0.0f, 4.0f},
                    state,
                    config,
                    platform);
    const game::SectorFpsVerticalResult landed = game::UpdateSectorFpsVerticalPhysics(
            state,
            config,
            landingContext,
            0.2f);
    Check(landed.transition == game::SectorFpsVerticalTransition::Landed
                  && state.grounded
                  && Near(state.feetPosition.y, 0.5f),
          "falling player lands and grounds on a static model top");

    state.feetPosition.x = 3.0f;
    const game::SectorFpsVerticalContext offContext =
            game::BuildSectorStaticModelVerticalContext(
                    game::SectorFpsVerticalContext{true, 0.0f, 4.0f},
                    state,
                    config,
                    platform);
    const game::SectorFpsVerticalResult walkedOff =
            game::UpdateSectorFpsVerticalPhysics(
                    state,
                    config,
                    offContext,
                    0.0f);
    Check(walkedOff.transition == game::SectorFpsVerticalTransition::StartedDrop
                  && !state.grounded,
          "walking off a static model starts a fall to the sector floor");

    const std::vector<game::SectorStaticModelCollider> overhead{
            MakeStaticModelCollider(
                    Vector2{0.0f, 0.0f},
                    Vector2{1.0f, 1.0f},
                    1.5f,
                    2.0f)};
    state = game::SectorFpsControllerState{};
    state.currentSectorId = 10;
    state.verticalVelocity = 4.0f;
    config.gravity = 1.0f;
    const game::SectorFpsVerticalContext overheadContext =
            game::BuildSectorStaticModelVerticalContext(
                    game::SectorFpsVerticalContext{true, 0.0f, 4.0f},
                    state,
                    config,
                    overhead);
    const game::SectorFpsVerticalResult bonked =
            game::UpdateSectorFpsVerticalPhysics(
                    state,
                    config,
                    overheadContext,
                    0.2f);
    Check(bonked.transition == game::SectorFpsVerticalTransition::CeilingBonk
                  && Near(state.feetPosition.y, 0.5f)
                  && Near(state.verticalVelocity, 0.0f),
          "jumping player collides with an elevated static model underside");
}

} // namespace

void TestSectorDoorAudioTransitionTracksTargetChanges()
{
    game::SectorDoorAudio audio;
    game::SectorDoorMotion motion;

    Check(game::UpdateSectorDoorAudioTransition(audio, motion)
                  == game::SectorDoorAudioEvent::None,
          "door audio does not fire for its initial closed target");
    motion.targetOpenFraction = 1.0f;
    Check(game::UpdateSectorDoorAudioTransition(audio, motion)
                  == game::SectorDoorAudioEvent::Open
                  && audio.pendingEvent == game::SectorDoorAudioEvent::Open
                  && audio.targetWasOpen,
          "door audio queues open when the target changes to open");
    Check(game::UpdateSectorDoorAudioTransition(audio, motion)
                  == game::SectorDoorAudioEvent::None
                  && audio.pendingEvent == game::SectorDoorAudioEvent::Open,
          "door audio keeps a pending event without retriggering a stable target");
    motion.targetOpenFraction = 0.0f;
    Check(game::UpdateSectorDoorAudioTransition(audio, motion)
                  == game::SectorDoorAudioEvent::Close
                  && audio.pendingEvent == game::SectorDoorAudioEvent::Close
                  && !audio.targetWasOpen,
          "door audio replaces the pending event on a close reversal");
}

void TestSectorDoorAudioEventTiming()
{
    game::SectorDoorMotion motion;
    motion.motion = game::SectorDoorMotionType::Swing;
    motion.openFraction = 1.0f;

    Check(game::IsSectorDoorAudioEventReady(
                      game::SectorDoorAudioEvent::Open, motion),
          "swing door open audio remains ready at the start of animation");
    Check(!game::IsSectorDoorAudioEventReady(
                      game::SectorDoorAudioEvent::Close, motion),
          "swing door close audio waits while the closing animation starts");

    motion.openFraction = 0.5f;
    Check(!game::IsSectorDoorAudioEventReady(
                      game::SectorDoorAudioEvent::Close, motion),
          "swing door close audio waits during the closing animation");
    motion.openFraction = std::numeric_limits<float>::epsilon();
    Check(!game::IsSectorDoorAudioEventReady(
                      game::SectorDoorAudioEvent::Close, motion),
          "swing door close audio waits until the exact closed endpoint");
    motion.openFraction = 0.0f;
    Check(game::IsSectorDoorAudioEventReady(
                      game::SectorDoorAudioEvent::Close, motion),
          "swing door close audio becomes ready when the leaf reaches the frame");

    for (const game::SectorDoorMotionType slide : {
                 game::SectorDoorMotionType::SlideVertical,
                 game::SectorDoorMotionType::SlideLeft,
                 game::SectorDoorMotionType::SlideRight}) {
        motion.motion = slide;
        motion.openFraction = 1.0f;
        Check(game::IsSectorDoorAudioEventReady(
                          game::SectorDoorAudioEvent::Close, motion),
              "sliding door close audio retains immediate timing");
    }

    game::SectorDoorAudio audio;
    audio.targetWasOpen = true;
    motion.motion = game::SectorDoorMotionType::Swing;
    motion.openFraction = 0.5f;
    motion.targetOpenFraction = 0.0f;
    Check(game::UpdateSectorDoorAudioTransition(audio, motion)
                          == game::SectorDoorAudioEvent::Close
                  && !game::IsSectorDoorAudioEventReady(audio.pendingEvent, motion),
          "swing door close request queues a deferred frame-hit sound");
    motion.targetOpenFraction = 1.0f;
    Check(game::UpdateSectorDoorAudioTransition(audio, motion)
                          == game::SectorDoorAudioEvent::Open
                  && audio.pendingEvent == game::SectorDoorAudioEvent::Open
                  && game::IsSectorDoorAudioEventReady(audio.pendingEvent, motion),
          "swing door reversal replaces the deferred close sound with open audio");

    motion.openFraction = std::numeric_limits<float>::quiet_NaN();
    Check(!game::IsSectorDoorAudioEventReady(
                      game::SectorDoorAudioEvent::Close, motion),
          "invalid swing pose does not dispatch a close impact sound");
    Check(!game::IsSectorDoorAudioEventReady(
                      game::SectorDoorAudioEvent::None, motion),
          "empty door audio event is never dispatchable");
}

void TestSectorDoorModelDrawPolicyTransitions()
{
    game::SectorDoorModelRender model;
    Check(game::ResolveSectorDoorModelDrawPolicy(model, false, false).drawProcedural,
          "procedural door model state draws the fallback slab");

    model.modelVisualRequested = true;
    model.catalogResolved = true;
    model.fallbackReason = game::SectorDoorModelFallbackReason::None;
    model.leafModel = engine::ModelHandle{1, 1};
    model.frameModel = engine::ModelHandle{2, 1};
    model.frameDeclared = true;
    game::SectorDoorModelDrawPolicy policy =
            game::ResolveSectorDoorModelDrawPolicy(model, false, false);
    Check(policy.drawProcedural && !policy.drawLeaf && !policy.drawFrame,
          "pending model leaf keeps the procedural fallback visible");

    model.leafReady = true;
    policy = game::ResolveSectorDoorModelDrawPolicy(model, true, false);
    Check(!policy.drawProcedural && policy.drawLeaf && !policy.drawFrame,
          "ready leaf replaces the fallback independently of its frame");

    model.frameReady = true;
    policy = game::ResolveSectorDoorModelDrawPolicy(model, true, true);
    Check(!policy.drawProcedural && policy.drawLeaf && policy.drawFrame,
          "ready leaf and frame are both model-render eligible");

    model.leafReady = false;
    model.leafFailed = true;
    policy = game::ResolveSectorDoorModelDrawPolicy(model, false, true);
    Check(policy.drawProcedural && !policy.drawLeaf && policy.drawFrame,
          "failed leaf keeps the slab while an independently ready frame remains visible");

    model.leafReady = true;
    model.leafFailed = false;
    model.frameFailed = true;
    policy = game::ResolveSectorDoorModelDrawPolicy(model, true, false);
    Check(!policy.drawProcedural && policy.drawLeaf && !policy.drawFrame,
          "failed frame never hides a ready model leaf");
}

void TestSectorDoorModelVisibilityUsesEitherAdjacentSector()
{
    game::SectorDoorResolvedAnchor anchor;
    anchor.frontSectorId = 10;
    anchor.backSectorId = 20;
    game::RuntimePortalVisibilityResult visibility;
    visibility.validStartSector = true;
    visibility.visibleSectorIds = {10};
    Check(game::ShouldDrawSectorDoorForVisibility(anchor, visibility),
          "model door is visible from its front adjacent sector");
    visibility.visibleSectorIds = {20};
    Check(game::ShouldDrawSectorDoorForVisibility(anchor, visibility),
          "model door is visible from its back adjacent sector");
    visibility.visibleSectorIds = {30};
    Check(!game::ShouldDrawSectorDoorForVisibility(anchor, visibility),
          "model door is culled when neither adjacent sector is visible");
    visibility.fallbackDrawAll = true;
    Check(game::ShouldDrawSectorDoorForVisibility(anchor, visibility),
          "fallback visibility conservatively includes model doors");
}

void TestSectorDoorModelBoundsAndLightingSectorHelpers()
{
    const BoundingBox local{
            Vector3{0.0f, 0.0f, -0.25f},
            Vector3{1.0f, 2.0f, 0.25f}};
    const BoundingBox moved = game::TransformSectorDoorModelBounds(
            local, MatrixTranslate(3.0f, 4.0f, 5.0f));
    Check(Near(moved.min, Vector3{3.0f, 4.0f, 4.75f})
                  && Near(moved.max, Vector3{4.0f, 6.0f, 5.25f}),
          "model door bounds transform all local AABB corners");
    const BoundingBox combined = game::UnionSectorDoorModelBounds(
            moved,
            BoundingBox{Vector3{2.0f, 3.0f, 5.0f}, Vector3{3.5f, 7.0f, 8.0f}});
    Check(Near(combined.min, Vector3{2.0f, 3.0f, 4.75f})
                  && Near(combined.max, Vector3{4.0f, 7.0f, 8.0f}),
          "model door receiver bounds union leaf and frame extents");

    game::SectorDoorResolvedAnchor anchor;
    anchor.frontSectorId = 10;
    anchor.backSectorId = 20;
    anchor.midpoint = Vector2{1.0f, 2.0f};
    anchor.normal = Vector2{0.0f, 1.0f};
    Check(game::ResolveSectorDoorAdjacentLightingSector(
                  anchor, Vector3{1.0f, 0.0f, 3.0f}, 20) == 20,
          "door lighting keeps a collision lookup result in an adjacent sector");
    Check(game::ResolveSectorDoorAdjacentLightingSector(
                  anchor, Vector3{1.0f, 0.0f, 3.0f}, 99) == 20,
          "door lighting falls back to the back sector from leaf position");
    Check(game::ResolveSectorDoorAdjacentLightingSector(
                  anchor, Vector3{1.0f, 0.0f, 1.0f}, 0) == 10,
          "door lighting falls back to the front sector from leaf position");
}

void TestSectorDoorModelShadowCasterMatrices()
{
    game::SectorObject object;
    game::SectorDoor door{91, true};
    game::SectorDoorRender render;
    render.visible = true;
    game::SectorDoorModelRender model;
    model.leafModel = engine::ModelHandle{3, 1};
    model.frameModel = engine::ModelHandle{4, 1};
    model.leafMatrix = MatrixTranslate(1.0f, 2.0f, 3.0f);
    model.frameMatrix = MatrixTranslate(4.0f, 5.0f, 6.0f);
    std::vector<game::SectorDoorModelShadowCaster> casters;
    const bool appended = game::AppendSectorDoorModelShadowCasters(
            engine::Entity{7, 1},
            object,
            door,
            render,
            model,
            game::SectorDoorModelDrawPolicy{false, true, true},
            casters);
    Check(appended && casters.size() == 2,
          "ready leaf and frame append one model shadow caster each");
    Check(casters[0].model == model.leafModel
                  && NearTranslation(casters[0].transform, Vector3{1.0f, 2.0f, 3.0f})
                  && casters[1].model == model.frameModel
                  && NearTranslation(casters[1].transform, Vector3{4.0f, 5.0f, 6.0f}),
          "model shadow casters preserve the canonical leaf and frame matrices");

    casters.clear();
    game::AppendSectorDoorModelShadowCasters(
            engine::Entity{7, 1},
            object,
            door,
            render,
            model,
            game::SectorDoorModelDrawPolicy{true, false, true},
            casters);
    Check(casters.size() == 1 && casters[0].model == model.frameModel,
          "fallback slab state emits only independently drawable model frame casters");
}

int main()
{
    extern void RunSectorScriptBindingTests();
    RunSectorScriptBindingTests();
    TestResolveSectorDoorAnchorValidPortal();
    TestResolveSectorDoorAnchorRejectsOneSidedWall();
    TestResolveSectorDoorAnchorRejectsSectorMismatch();
    TestResolveSectorDoorAnchorRejectsZeroHeightOpening();
    TestResolveSectorDoorAnchorUsesAuthoredDimensionsWhenPresent();
    TestResolveSectorDoorAnchorToleratesStaleEndpointDiagnostics();
    TestResolveSectorDoorAnchorReversedEndpointOrderPreservesFrontBackNormal();
    TestResolveSectorDoorAnchorHorizontalPortalBasis();
    TestSectorRuntimeObjectComponentsIterateAndDestroy();
    TestSectorBillboardFrameUvsUseSourceRectangle();
    TestSectorBillboardFrameUvsPreserveFlippedSourceSigns();
    TestSectorBillboardQuadWorldPositions();
    TestClearSectorRuntimeObjectsOnlyDestroysSectorObjects();
    TestSpawnPlacedDoorPositiveNormalOffsetMovesTowardBackSector();
    TestSpawnPlacedDoorHeightOffsetMovesSlabAndColliderWithoutResizing();
    TestRefreshSectorRuntimeObjectMapDataReportsDoorAnchorDiagnostics();
    TestSpawnPlacedRuntimeObjectSkipsInvalidDoorAnchorWithDiagnostics();
    TestSpawnPlacedDoorCopiesResolvedPayloadToEcs();
    TestSpawnPlacedDoorRefreshDoesNotDuplicate();
    TestUnknownModelSwingDoorUsesAnimatedProceduralFallback();
    TestKnownModelSwingDoorUsesUniformCatalogFallbackDimensions();
    TestRetainedCatalogFailureDoesNotBlockOtherDoors();
    TestNullSwingDoorLeafRequestKeepsRuntimeFallback();
    TestProceduralSwingDoorRunsWithoutCatalogStyle();
    TestMixedDoorRuntimeRefreshRecoveryAndScopeLifecycle();
    TestSectorDoorSlabGeometryIsFiniteAndStable();
    TestSectorDoorSlabMeshDataHasStableAttributes();
    TestSectorDoorFaceUvsAffectOnlySelectedFace();
    TestSectorDoorFaceUvHelpers();
    TestSectorDoorFitBothMeshUvsSpanOnce();
    TestSpawnPlacedDoorCopiesFaceUvsWithoutChangingPhysicalState();
    TestSectorDoorSlabModelMatrixPreservesResolvedBasis();
    TestSectorDoorStaticLightingColorsSamplePerVertexProbes();
    TestSectorDoorStaticLightingColorsFallbackSafely();
    TestSectorDoorStaticLightingColorsDoNotMutateGeometry();
    TestSectorDoorReceiverBoundsUseAnimatedSlabGeometry();
    TestSectorDoorReceiverBoundsSkipNonRenderableDoors();
    TestSectorDoorShadowCasterCollectionIncludesValidDoor();
    TestSectorDoorShadowCasterCollectionSkipsNonRenderableDoors();
    TestSectorDoorShadowCasterUsesAnimatedTransform();
    TestSectorDoorHorizontalSlideMotionUsesResolvedTangent();
    TestSectorSwingDoorCanonicalPoseKeepsHingesAndChoosesSide();
    TestSectorSwingDoorDerivedMatricesAndColliderAgree();
    TestSectorSwingDoorFullyOpenClearsApertureButStillCollides();
    TestSectorSwingDoorClosingSweepReopensWithoutTunneling();
    TestSectorDoorNavigationHoldsAndSlidingObstruction();
    TestNpcDoorTraversalStagesWaitsCrossesAndReleases();
    TestSpawnPlacedDoorDerivesDefaultOpenDistance();
    TestSectorDoorMotionAdvancesOpenAndClosed();
    TestSectorDoorMotionClampsAndIgnoresZeroSpeed();
    TestSectorDoorAudioTransitionTracksTargetChanges();
    TestSectorDoorAudioEventTiming();
    TestSectorDoorModelDrawPolicyTransitions();
    TestSectorDoorModelVisibilityUsesEitherAdjacentSector();
    TestSectorDoorModelBoundsAndLightingSectorHelpers();
    TestSectorDoorModelShadowCasterMatrices();
    TestSectorDoorAutoOpenSetsTargetFromPlayerRange();
    TestSectorDoorAutoOpenIgnoresDisabledAndInvalidPlayerPosition();
    TestSectorDoorInteractTogglesNearestManualDoorInFront();
    TestSectorDoorInteractRequiresFacingAndTogglesClosed();
    TestSectorSideSlidingDoorInteractUsesStablePortalAnchor();
    TestSectorSideSlidingDoorAutoOpenUsesStablePortalAnchor();
    TestSectorDoorDerivedStateUpdatesTransformAndCollider();
    TestSectorDoorDerivedStateUpdatesLeftSlideAndBlockerThreshold();
    TestSectorDoorDynamicColliderCollectionIncludesEnabledDoorShapes();
    TestSectorDoorDynamicColliderCollectionExcludesDisabledAndInvalidShapes();
    TestSectorDoorDynamicCollisionBlocksClosedDoor();
    TestSectorDoorDynamicCollisionBlocksThinDoorTunneling();
    TestSectorDoorDynamicCollisionPreservesTangentialSlide();
    TestSectorDoorDynamicCollisionRotatedSlide();
    TestSectorDoorDynamicCollisionContactAllowsTangentAndAwayMovement();
    TestSectorDoorDynamicCollisionRoundedCornerSweep();
    TestSectorDoorDynamicCollisionIgnoresPortalBlockerState();
    TestSectorDoorDynamicPortalBlockerCollectionBuildsDirectedVisibilityKeys();
    TestSpawnedDoorRuntimeUpdateRefreshesPortalBlockerCollection();
    TestSectorDoorDynamicCollisionAllowsVerticalNonOverlap();
    TestSectorDoorDynamicCollisionAllowsPhysicallyClearCrossing();
    TestSectorDoorDynamicCollisionStartsInsideSafe();
    TestDoorAnchorDiagnosticsDoNotAffectValidBillboardRuntimeObject();
    TestSpawnPlacedRuntimeObjectSkipsLegacyGoblinDefinition();
    TestSpawnPlacedRuntimeObjectSkipsUnsupportedKind();
    TestSpawnPlacedRuntimeObjectSkipsMissingBillboardSprite();
    TestSpawnPlacedBillboardCopiesAuthoredPayloadToEcs();
    TestSpawnPlacedStaticModelCopiesAuthoredPayloadToEcs();
    TestSectorModelEnvironmentExposureFollowsSectorLighting();
    TestSpawnUnassignedStaticModelRemainsSelectableRuntimeEntity();
    TestSpawnDynamicModelCopiesPlaybackAndLightingPayload();
    TestSpawnNpcResolvesDefinitionAndIdlePlayback();
    TestNpcNavigationRoutesIndependentAgentsAroundStaticCollision();
    TestNpcNavigationLowSpeedAdvancesAtHighRefreshRate();
    TestNpcSemanticAnimationUsesBlendingAndQueuesTransitions();
    TestNpcNavigationSmoothsSectorGeometryStairsVisually();
    TestSpawnNpcMissingDefinitionRemainsDiagnosticSkip();
    TestAnimatedModelSelectionAndBlendApi();
    TestStaticModelAuxiliaryMaterialMapsBindDrawMeshTextures();
    TestStaticModelSpotlightShadowCasterCollectionAndRevision();
    TestViewmodelMaterialOverrideResolution();
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
    TestPropAndDoorStandingClearanceQueries();
    TestStaticModelColliderBuildUsesFullAuthoredTransform();
    TestStaticModelXzRotationUsesSharedTransformAndEnclosingBounds();
    TestStaticModelColliderBlocksSweepsAndSlides();
    TestStaticModelColliderContactAllowsEscapeAndTangentMovement();
    TestStaticModelColliderRoundedCornerSweep();
    TestStaticModelColliderRotatedContactAndPenetrationRecovery();
    TestStaticModelColliderRepeatedContactCanBackAway();
    TestStaticModelColliderUsesVerticalOverlapAndStepHeight();
    TestStaticModelVerticalContextSelectsNearestSurfaces();
    TestStaticModelColliderLandingUndersideAndWalkOff();

    if (failures != 0) {
        std::fprintf(stderr, "%d sector runtime object test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
