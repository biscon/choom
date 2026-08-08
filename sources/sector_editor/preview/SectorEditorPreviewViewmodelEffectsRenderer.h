#pragma once

#include "game/FpsViewmodel.h"

#include <raylib.h>

namespace game {

Color EvaluateSectorEditorPreviewMuzzleFlashGradient(
        const FpsMuzzleFlashRuntimeState& flash,
        float normalizedRadius,
        float lifeAmount);

void DrawSectorEditorPreviewMuzzleFlash(
        const FpsWeaponFiringRuntimeState& firing,
        const Camera3D& viewmodelCamera);

} // namespace game
