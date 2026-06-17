#pragma once

#include "engine/ecs/World.h"

namespace game {

void RectangleBounceSystem(engine::World& world, float width, float height);
void SpriteRendererBounceSystem(engine::World& world, float width, float height);

} // namespace game
