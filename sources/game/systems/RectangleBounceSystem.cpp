#include "game/systems/RectangleBounceSystem.h"

#include "engine/components/SpriteRenderer.h"
#include "engine/components/Transform.h"
#include "engine/components/Velocity.h"
#include "game/components/Rectangle.h"

namespace game {

void RectangleBounceSystem(engine::World& world, float width, float height)
{
    world.ForEach<engine::Transform, engine::Velocity, game::Rectangle>(
            [width, height](
                    engine::Entity,
                    engine::Transform& transform,
                    engine::Velocity& velocity,
                    game::Rectangle& rectangle) {
                const float halfWidth = rectangle.size.x * transform.scale.x * 0.5f;
                const float halfHeight = rectangle.size.y * transform.scale.y * 0.5f;

                if (transform.position.x < halfWidth) {
                    transform.position.x = halfWidth;
                    velocity.value.x = -velocity.value.x;
                } else if (transform.position.x > width - halfWidth) {
                    transform.position.x = width - halfWidth;
                    velocity.value.x = -velocity.value.x;
                }

                if (transform.position.y < halfHeight) {
                    transform.position.y = halfHeight;
                    velocity.value.y = -velocity.value.y;
                } else if (transform.position.y > height - halfHeight) {
                    transform.position.y = height - halfHeight;
                    velocity.value.y = -velocity.value.y;
                }
            }
    );
}

void SpriteRendererBounceSystem(engine::World& world, float width, float height)
{
    world.ForEach<engine::Transform, engine::Velocity, engine::SpriteRenderer>(
            [width, height](
                    engine::Entity,
                    engine::Transform& transform,
                    engine::Velocity& velocity,
                    engine::SpriteRenderer& renderer) {
                const float halfWidth = renderer.size.x * transform.scale.x * 0.5f;
                const float halfHeight = renderer.size.y * transform.scale.y * 0.5f;

                if (transform.position.x < halfWidth) {
                    transform.position.x = halfWidth;
                    velocity.value.x = -velocity.value.x;
                } else if (transform.position.x > width - halfWidth) {
                    transform.position.x = width - halfWidth;
                    velocity.value.x = -velocity.value.x;
                }

                if (transform.position.y < halfHeight) {
                    transform.position.y = halfHeight;
                    velocity.value.y = -velocity.value.y;
                } else if (transform.position.y > height - halfHeight) {
                    transform.position.y = height - halfHeight;
                    velocity.value.y = -velocity.value.y;
                }
            }
    );
}

} // namespace game
