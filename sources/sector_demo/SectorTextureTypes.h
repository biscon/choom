#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/TextureLoadFlags.h"

#include <cctype>
#include <string>
#include <string_view>

namespace game {

enum class SectorMaterialFilter {
    Point,
    Bilinear,
    Trilinear,
    Anisotropic8x
};

enum class SectorMaterialPropertyMapKind {
    None = 0,
    Roughness = 1,
    Orm = 2
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
std::string SectorMaterialOrmMapPath(const std::string& baseTexturePath);
std::string SectorMaterialRoughnessMapPath(const std::string& baseTexturePath);

inline bool HasSectorMaterialCompanionMarker(
        const std::string& texturePath,
        std::string_view marker)
{
    const std::size_t separator = texturePath.find_last_of("/\\");
    const std::size_t fileNameBegin = separator == std::string::npos
            ? 0
            : separator + 1;
    const std::size_t extension = texturePath.find_last_of('.');
    const std::size_t stemEnd = extension == std::string::npos
                    || extension <= fileNameBegin
            ? texturePath.size()
            : extension;
    for (std::size_t position = fileNameBegin;
            position + marker.size() <= stemEnd;
            ++position) {
        bool matches = true;
        for (std::size_t i = 0; i < marker.size(); ++i) {
            matches = matches
                    && std::tolower(static_cast<unsigned char>(texturePath[position + i]))
                            == std::tolower(static_cast<unsigned char>(marker[i]));
        }
        const std::size_t markerEnd = position + marker.size();
        if (matches && (markerEnd == stemEnd || texturePath[markerEnd] == '_')) {
            return true;
        }
    }
    return false;
}

inline bool IsSectorMaterialNormalMapPath(const std::string& texturePath)
{
    return HasSectorMaterialCompanionMarker(texturePath, "_normal");
}

inline bool IsSectorMaterialOrmMapPath(const std::string& texturePath)
{
    return HasSectorMaterialCompanionMarker(texturePath, "_orm");
}

inline bool IsSectorMaterialRoughnessMapPath(const std::string& texturePath)
{
    return HasSectorMaterialCompanionMarker(texturePath, "_roughness");
}

inline bool IsSectorMaterialCompanionMapPath(const std::string& texturePath)
{
    return IsSectorMaterialNormalMapPath(texturePath)
            || IsSectorMaterialOrmMapPath(texturePath)
            || IsSectorMaterialRoughnessMapPath(texturePath);
}

} // namespace game
