#pragma once

#include "engine/EngineContext.h"
#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "sector_demo/SectorFreeflyController.h"
#include "sector_demo/SectorMeshPreview.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"

namespace game {

class SectorDemo {
public:
    bool Init(engine::EngineContext& context, const char* mapPath);
    void Shutdown(engine::EngineContext& context);

    void Update(engine::EngineContext& context, float dt);
    void Render(engine::EngineContext& context);
    void RenderOverlay(engine::AssetManager& assets);

private:
    SectorMeshPreview preview;
    SectorRuntimeObjectState runtimeObjects;
    SectorTopologyMap topologyMap;
    SectorFreeflyControllerState freeflyController;
    bool initialized = false;
};

} // namespace game
