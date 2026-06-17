#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "engine/input/Input.h"

namespace engine {

struct EngineContext {
    World world;
    AssetManager assets;
    Input input;
};

} // namespace engine
