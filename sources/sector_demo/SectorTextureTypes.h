#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/TextureLoadFlags.h"

#include <string>

namespace game {

enum class SectorTextureFilter {
    Point,
    Bilinear
};

struct SectorTextureDefinition {
    std::string id;
    std::string path;
    SectorTextureFilter filter = SectorTextureFilter::Bilinear;
};

struct SectorTextureBinding {
    std::string textureId;
    engine::TextureHandle handle = engine::NullTextureHandle();
};

engine::TextureLoadFlags SectorTextureLoadFlags(SectorTextureFilter filter);
const char* SectorTextureFilterName(SectorTextureFilter filter);

} // namespace game
