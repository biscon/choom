#include "sector_demo/SectorTextureTypes.h"

#include <filesystem>

namespace game {

engine::TextureLoadFlags SectorTextureLoadFlags(SectorTextureFilter filter)
{
    switch (filter) {
        case SectorTextureFilter::Point:
            return engine::TextureLoad_PointFilter;
        case SectorTextureFilter::Bilinear:
            return engine::TextureLoad_BilinearFilter;
        case SectorTextureFilter::Trilinear:
            return engine::TextureLoad_TrilinearFilter;
        case SectorTextureFilter::Anisotropic8x:
            return engine::TextureLoad_Anisotropic8x;
    }
    return engine::TextureLoad_Anisotropic8x;
}

const char* SectorTextureFilterName(SectorTextureFilter filter)
{
    switch (filter) {
        case SectorTextureFilter::Point:
            return "point";
        case SectorTextureFilter::Bilinear:
            return "bilinear";
        case SectorTextureFilter::Trilinear:
            return "trilinear";
        case SectorTextureFilter::Anisotropic8x:
            return "aniso 8x";
    }
    return "aniso 8x";
}

std::string SectorTextureNormalMapPath(const std::string& baseTexturePath)
{
    if (baseTexturePath.empty()) {
        return {};
    }

    const std::filesystem::path path(baseTexturePath);
    const std::filesystem::path fileName = path.filename();
    if (fileName.empty()) {
        return {};
    }

    const std::string normalFileName = fileName.stem().string()
            + "_normal"
            + fileName.extension().string();
    return (path.parent_path() / normalFileName).generic_string();
}

} // namespace game
