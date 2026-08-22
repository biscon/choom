#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/TextureLoadFlags.h"

#include <string>

namespace game {

enum class SectorMaterialFilter {
    Point,
    Bilinear,
    Trilinear,
    Anisotropic8x
};

struct SectorMaterialDefinition {
    std::string id;
    std::string path;
    SectorMaterialFilter filter = SectorMaterialFilter::Anisotropic8x;
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.8f;
    float normalStrength = 1.0f;
};

struct SectorTextureBinding {
    std::string materialId;
    engine::TextureHandle handle = engine::NullTextureHandle();
};

engine::TextureLoadFlags SectorMaterialTextureLoadFlags(SectorMaterialFilter filter);
const char* SectorMaterialFilterName(SectorMaterialFilter filter);
std::string SectorMaterialNormalMapPath(const std::string& baseTexturePath);
inline bool IsSectorMaterialNormalMapPath(const std::string& texturePath)
{
    const std::size_t separator = texturePath.find_last_of("/\\");
    const std::size_t fileNameBegin = separator == std::string::npos
            ? 0
            : separator + 1;
    const std::size_t extension = texturePath.find_last_of('.');
    const std::size_t stemEnd = extension == std::string::npos || extension <= fileNameBegin
            ? texturePath.size()
            : extension;
    constexpr const char* NormalMarker = "_normal";
    constexpr std::size_t NormalMarkerLength = 7;
    std::size_t marker = texturePath.find(NormalMarker, fileNameBegin);
    while (marker != std::string::npos && marker + NormalMarkerLength <= stemEnd) {
        const std::size_t markerEnd = marker + NormalMarkerLength;
        if (markerEnd == stemEnd || texturePath[markerEnd] == '_') {
            return true;
        }
        marker = texturePath.find(NormalMarker, markerEnd);
    }
    return false;
}

} // namespace game
