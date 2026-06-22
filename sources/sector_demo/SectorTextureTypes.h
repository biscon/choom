#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/TextureLoadFlags.h"

#include <string>

namespace game {

enum class SectorTextureFilter {
    Point,
    Bilinear,
    Trilinear,
    Anisotropic8x
};

struct SectorTextureDefinition {
    std::string id;
    std::string path;
    SectorTextureFilter filter = SectorTextureFilter::Anisotropic8x;
};

struct SectorTextureBinding {
    std::string textureId;
    engine::TextureHandle handle = engine::NullTextureHandle();
};

engine::TextureLoadFlags SectorTextureLoadFlags(SectorTextureFilter filter);
const char* SectorTextureFilterName(SectorTextureFilter filter);

} // namespace game
