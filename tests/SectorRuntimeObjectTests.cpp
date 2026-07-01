#include "sector_demo/SectorRuntimeObjects.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

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
    state.temporaryGoblinDebugSpawnEntity = engine::NullEntity();

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
    Check(engine::IsNull(state.temporaryGoblinDebugSpawnEntity),
            "sector runtime cleanup clears temporary goblin entity handle");
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
    TestSectorRuntimeObjectComponentsIterateAndDestroy();
    TestSectorBillboardFrameUvsUseSourceRectangle();
    TestSectorBillboardFrameUvsPreserveFlippedSourceSigns();
    TestSectorBillboardQuadWorldPositions();
    TestClearSectorRuntimeObjectsOnlyDestroysSectorObjects();
    TestSectorBillboardSpriteAnimationRequestRejectsMissingInput();
    TestSectorBillboardAnimatorAdvances();
    TestSectorBillboardDirectionalClipsResolve();
    TestSectorBillboardDirectionalClipsFallback();
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
