#include "sector_demo/SectorDemo.h"

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologySerialization.h"

#include <raylib.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace game {

namespace {

Vector3 HorizontalForwardFromYaw(float yawRadians)
{
    return Vector3{std::cos(yawRadians), 0.0f, std::sin(yawRadians)};
}

} // namespace

bool SectorDemo::Init(engine::EngineContext& context, const char* mapPath)
{
    Shutdown(context);

    engine::AssetManager& assets = context.assets;
    std::string error;
    if (!LoadSectorTopologyMap(mapPath, topologyMap, &error)) {
        std::fprintf(stderr, "[SectorDemo ERROR] %s\n", error.c_str());
        return false;
    }

    ResetSectorRuntimeObjectsForMap(context.world, assets, runtimeObjects, topologyMap);

    if (!preview.RebuildRendererResources(assets, topologyMap, "sector_demo", error)) {
        ClearSectorRuntimeObjects(context.world, assets, runtimeObjects);
        std::fprintf(stderr, "[SectorDemo ERROR] %s\n", error.c_str());
        return false;
    }

    ResetSectorFreeflyController(freeflyController, preview.RendererPose());
    EnterSectorFreeflyController(freeflyController);
    preview.ApplyRendererPose(freeflyController.pose);
    initialized = true;
    return true;
}

void SectorDemo::Shutdown(engine::EngineContext& context)
{
    if (initialized) {
        LeaveSectorFreeflyController();
    }
    engine::AssetManager& assets = context.assets;
    if (initialized
            || runtimeObjects.worldReserved
            || !engine::IsNull(runtimeObjects.runtimeObjectAssetScope)) {
        ClearSectorRuntimeObjects(context.world, assets, runtimeObjects);
    }
    preview.ShutdownRendererResources(assets);
    topologyMap = SectorTopologyMap{};
    initialized = false;
}

void SectorDemo::Update(engine::EngineContext& context, float dt)
{
    if (!initialized) {
        return;
    }

    engine::Input& input = context.input;
    const Vector3 playerPosition = freeflyController.pose.position;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key != KEY_F4) {
                    return;
                }
                preview.ToggleDynamicLightingEnabled();
                engine::ConsumeEvent(event);
            });
    if (freeflyController.mouseLookEnabled) {
        input.ForEachEvent(
                engine::InputEventType::KeyPressed,
                true,
                [this, &context, &playerPosition](engine::InputEvent& event) {
                    if (event.key.key != KEY_F) {
                        return;
                    }
                    if (ToggleTargetedSectorDoorInteractionSystem(
                                context.world,
                                playerPosition,
                                HorizontalForwardFromYaw(freeflyController.pose.yawRadians))) {
                        engine::ConsumeEvent(event);
                    }
                });
    }
    UpdateSectorRuntimeObjects(context.world, context.assets, runtimeObjects, topologyMap, dt, &playerPosition);
    preview.AdvanceRuntime(dt);

    UpdateSectorFreeflyController(freeflyController, input, dt);
    preview.ApplyRendererPose(freeflyController.pose);
    preview.UpdateVisibilityDebug(0, 0.0f, false, nullptr, &context.world);
}

void SectorDemo::Render(engine::EngineContext& context)
{
    preview.DrawScene(
            context.assets,
            true,
            &context.world,
            SectorRuntimeDoorLightingContext{&runtimeObjects.objectLightProbes, &topologyMap});
}

void SectorDemo::RenderOverlay(engine::AssetManager& assets)
{
    if (!initialized) {
        return;
    }

    const Vector3 position = preview.Position();
    DrawText("Sector Mesh Demo", 40, 36, 30, RAYWHITE);
    DrawText("WASD move  |  Mouse look  |  Space/Ctrl up/down  |  F interact  |  F4 dynamic lights  |  F11 cursor toggle", 40, 76, 20, LIGHTGRAY);
    DrawText(
            TextFormat(
                    "pos %.2f %.2f %.2f   sectors %zu   batches %zu   assets %.0f%%",
                    position.x,
                    position.y,
                    position.z,
                    preview.SectorCount(),
                    preview.BatchCount(),
                    preview.RendererAssetProgress(assets) * 100.0f
            ),
            40,
            106,
            20,
            LIGHTGRAY
    );
    DrawText(
            preview.VisibilityDebugText().c_str(),
            40,
            136,
            20,
            LIGHTGRAY
    );
}

} // namespace game
