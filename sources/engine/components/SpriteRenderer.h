#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

namespace engine {

struct SpriteRenderer {
    TextureHandle texture;
    Rectangle source = {};
    Vector2 size = {};
    Vector2 origin = {};
    Color tint = WHITE;
    bool flipX = false;
    bool flipY = false;
    bool visible = true;
};

} // namespace engine
