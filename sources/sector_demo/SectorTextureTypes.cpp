#include "sector_demo/SectorTextureTypes.h"

#include <filesystem>

namespace game {

engine::TextureLoadFlags SectorMaterialTextureLoadFlags(SectorMaterialFilter filter)
{
    switch (filter) {
        case SectorMaterialFilter::Point:
            return engine::TextureLoad_PointFilter;
        case SectorMaterialFilter::Bilinear:
            return engine::TextureLoad_BilinearFilter;
        case SectorMaterialFilter::Trilinear:
            return engine::TextureLoad_TrilinearFilter;
        case SectorMaterialFilter::Anisotropic8x:
            return engine::TextureLoad_Anisotropic8x;
    }
    return engine::TextureLoad_Anisotropic8x;
}

const char* SectorMaterialFilterName(SectorMaterialFilter filter)
{
    switch (filter) {
        case SectorMaterialFilter::Point:
            return "point";
        case SectorMaterialFilter::Bilinear:
            return "bilinear";
        case SectorMaterialFilter::Trilinear:
            return "trilinear";
        case SectorMaterialFilter::Anisotropic8x:
            return "aniso 8x";
    }
    return "aniso 8x";
}

std::string SectorMaterialNormalMapPath(const std::string& baseTexturePath)
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
