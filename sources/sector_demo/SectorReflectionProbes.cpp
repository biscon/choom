#include "sector_demo/SectorReflectionProbes.h"

namespace game {
int SectorReflectionProbeMipCount(int resolution)
{
    if (resolution != 64 && resolution != 128 && resolution != 256) return 0;
    int count = 0;
    for (int size = resolution; size; size /= 2) ++count;
    return count;
}
} // namespace game
