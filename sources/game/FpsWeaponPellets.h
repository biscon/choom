#pragma once

#include "game/FpsWeaponRegistry.h"

#include <raylib.h>

#include <cstdint>

namespace game {

Vector3 FpsWeaponPelletDirection(
        Vector3 aimDirection,
        const FpsWeaponPelletDefinition& definition,
        int pelletIndex,
        uint64_t shotSequence);

} // namespace game
