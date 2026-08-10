#include "game/FpsWeaponRegistry.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorAssetPaths.h"

#include <raylib.h>

namespace game {

void RequestFpsWeaponAudioAssets(
        engine::AssetManager& assets,
        FpsWeaponRegistry& registry)
{
    for (FpsWeaponDefinition& weapon : registry.weapons) {
        weapon.firing.shootSound = engine::NullSoundHandle();
        if (weapon.firing.shootSoundPath.empty()) continue;
        const std::string path = ResolveSectorAudioAssetPath(
                weapon.firing.shootSoundPath);
        weapon.firing.shootSound = assets.RequestSound(
                assets.GlobalScope(),
                path.c_str());
        if (engine::IsNull(weapon.firing.shootSound)) {
            TraceLog(
                    LOG_WARNING,
                    "Could not request weapon shoot sound: %s",
                    path.c_str());
        }
    }
}

} // namespace game
