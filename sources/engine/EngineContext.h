#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/ecs/World.h"
#include "engine/input/Input.h"

namespace engine {

struct EngineContext {
    World world;
    AudioSystem audio;
    AssetManager assets;
    Input input;
};

} // namespace engine
