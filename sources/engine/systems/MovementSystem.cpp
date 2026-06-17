#include "engine/systems/MovementSystem.h"

#include "engine/components/Transform.h"
#include "engine/components/Velocity.h"

namespace engine {

void MovementSystem(World& world, float dt)
{
    world.ForEach<Transform, Velocity>(
            [dt](Entity, Transform& transform, Velocity& velocity) {
                transform.position.x += velocity.value.x * dt;
                transform.position.y += velocity.value.y * dt;
            }
    );
}

} // namespace engine
