#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"

namespace engine {

void SpriteAnimatorSystem(World& world, AssetManager& assets, float dt);

} // namespace engine
