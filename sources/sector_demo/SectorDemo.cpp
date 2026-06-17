#include "sector_demo/SectorDemo.h"

#include "sector_demo/SectorMap.h"

#include <raylib.h>

#include <cstdio>
#include <string>

namespace game {

bool SectorDemo::Init(engine::AssetManager& assets, const char* mapPath)
{
    Shutdown(assets);

    SectorMap map;
    if (!LoadSectorMap(mapPath, map)) {
        return false;
    }

    std::string error;
    if (!preview.Rebuild(assets, map, "sector_demo", error)) {
        std::fprintf(stderr, "[SectorDemo ERROR] %s\n", error.c_str());
        return false;
    }

    initialized = true;
    return true;
}

void SectorDemo::Shutdown(engine::AssetManager& assets)
{
    preview.Shutdown(assets);
    initialized = false;
}

void SectorDemo::Update(engine::Input& input, float dt)
{
    preview.Update(input, dt);
}

void SectorDemo::Render(engine::AssetManager& assets)
{
    preview.Render(assets);
}

void SectorDemo::RenderOverlay(engine::AssetManager& assets)
{
    if (!initialized) {
        return;
    }

    const Vector3 position = preview.Position();
    DrawText("Sector Mesh Demo", 40, 36, 30, RAYWHITE);
    DrawText("WASD move  |  Mouse look  |  Space/Ctrl up/down  |  F11 cursor toggle", 40, 76, 20, LIGHTGRAY);
    DrawText(
            TextFormat(
                    "pos %.2f %.2f %.2f   sectors %zu   batches %zu   assets %.0f%%",
                    position.x,
                    position.y,
                    position.z,
                    preview.SectorCount(),
                    preview.BatchCount(),
                    preview.AssetProgress(assets) * 100.0f
            ),
            40,
            106,
            20,
            LIGHTGRAY
    );
}

} // namespace game
