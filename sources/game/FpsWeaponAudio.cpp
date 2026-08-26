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
        weapon.reload.dryFireSound = engine::NullSoundHandle();
        weapon.reload.reloadSound = engine::NullSoundHandle();
        const auto request = [&assets](
                const std::string& configuredPath,
                engine::SoundHandle& handle,
                const char* description) {
            if (configuredPath.empty()) return;
            const std::string path = ResolveSectorAudioAssetPath(configuredPath);
            handle = assets.RequestSound(assets.GlobalScope(), path.c_str());
            if (!engine::IsNull(handle)) return;
            TraceLog(
                    LOG_WARNING,
                    "Could not request weapon %s sound: %s",
                    description,
                    path.c_str());
        };
        request(
                weapon.firing.shootSoundPath,
                weapon.firing.shootSound,
                "shoot");
        request(
                weapon.reload.dryFireSoundPath,
                weapon.reload.dryFireSound,
                "dry-fire");
        request(
                weapon.reload.reloadSoundPath,
                weapon.reload.reloadSound,
                "reload");
    }
}

} // namespace game
