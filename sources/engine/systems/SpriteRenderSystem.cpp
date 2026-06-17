#include "engine/systems/SpriteRenderSystem.h"

#include "engine/components/SpriteRenderer.h"
#include "engine/components/Transform.h"

#include <raylib.h>
#include <cmath>

namespace engine {

void SpriteRenderSystem(World& world, AssetManager& assets)
{
    world.ForEach<Transform, SpriteRenderer>(
            [&assets](Entity, Transform& transform, SpriteRenderer& renderer) {
                if (!renderer.visible) {
                    return;
                }

                const Vector2 size{
                        renderer.size.x * transform.scale.x,
                        renderer.size.y * transform.scale.y
                };
                const Vector2 origin{
                        renderer.origin.x * transform.scale.x,
                        renderer.origin.y * transform.scale.y
                };
                const Rectangle dst{
                        std::round(transform.position.x),
                        std::round(transform.position.y),
                        size.x,
                        size.y
                };

                const Texture2D* texture = assets.GetTexture(renderer.texture);
                if (texture == nullptr) {
                    const Rectangle fallback{
                            std::round(transform.position.x - origin.x),
                            std::round(transform.position.y - origin.y),
                            size.x,
                            size.y
                    };
                    DrawRectangleLinesEx(fallback, 2.0f, MAGENTA);
                    return;
                }

                Rectangle source = renderer.source;
                if (source.width == 0.0f || source.height == 0.0f) {
                    source = Rectangle{
                            0.0f,
                            0.0f,
                            static_cast<float>(texture->width),
                            static_cast<float>(texture->height)
                    };
                }

                if (renderer.flipX) {
                    source.width = -source.width;
                }
                if (renderer.flipY) {
                    source.height = -source.height;
                }

                DrawTexturePro(*texture, source, dst, origin, transform.rotation, renderer.tint);
            }
    );
}

} // namespace engine
