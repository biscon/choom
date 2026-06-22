#include "sector_demo/SectorTextureTypes.h"

namespace game {

engine::TextureLoadFlags SectorTextureLoadFlags(SectorTextureFilter filter)
{
    switch (filter) {
        case SectorTextureFilter::Point:
            return engine::TextureLoad_PointFilter;
        case SectorTextureFilter::Bilinear:
            return engine::TextureLoad_BilinearFilter;
    }
    return engine::TextureLoad_BilinearFilter;
}

const char* SectorTextureFilterName(SectorTextureFilter filter)
{
    switch (filter) {
        case SectorTextureFilter::Point:
            return "point";
        case SectorTextureFilter::Bilinear:
            return "bilinear";
    }
    return "bilinear";
}

} // namespace game
