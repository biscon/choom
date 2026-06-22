#include "sector_demo/SectorTextureTypes.h"

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

} // namespace game
