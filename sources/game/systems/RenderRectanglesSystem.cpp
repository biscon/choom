#include "game/systems/RenderRectanglesSystem.h"

#include "engine/components/Transform.h"
#include "game/components/Rectangle.h"

#include <raylib.h>

namespace game {

void RenderRectanglesSystem(engine::World& world)
{
    world.ForEach<engine::Transform, game::Rectangle>(
            [](engine::Entity, engine::Transform& transform, game::Rectangle& rectangle) {
                const Vector2 size{
                        rectangle.size.x * transform.scale.x,
                        rectangle.size.y * transform.scale.y
                };
                const ::Rectangle rect{
                        transform.position.x,
                        transform.position.y,
                        size.x,
                        size.y
                };
                const Vector2 origin{size.x * 0.5f, size.y * 0.5f};

                DrawRectanglePro(rect, origin, transform.rotation, RAYWHITE);
            }
    );
}

} // namespace game
