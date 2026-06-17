#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "sector_demo/SectorMeshPreview.h"

namespace game {

class SectorDemo {
public:
    bool Init(engine::AssetManager& assets, const char* mapPath);
    void Shutdown(engine::AssetManager& assets);

    void Update(engine::Input& input, float dt);
    void Render(engine::AssetManager& assets);
    void RenderOverlay(engine::AssetManager& assets);

private:
    SectorMeshPreview preview;
    bool initialized = false;
};

} // namespace game
