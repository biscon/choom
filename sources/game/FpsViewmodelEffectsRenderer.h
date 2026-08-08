#pragma once

#include "game/FpsViewmodel.h"

#include <raylib.h>

namespace game {

struct FpsMuzzleFlashTemporalState {
    float normalizedAge = 0.0f;
    float expansionScale = 1.0f;
    float opacity = 1.0f;
    float warmth = 0.0f;
};

struct FpsMuzzleFlashRibbonAxes {
    bool valid = false;
    Vector3 first{};
    Vector3 second{};
};

FpsMuzzleFlashTemporalState
EvaluateFpsMuzzleFlashTemporalState(
        float ageSeconds,
        float lifetimeSeconds);

Color EvaluateFpsMuzzleFlashGradient(
        const FpsMuzzleFlashRuntimeState& flash,
        float normalizedRadius,
        float opacity,
        float warmth = 0.0f);

FpsMuzzleFlashRibbonAxes
BuildFpsMuzzleFlashRibbonAxes(
        Vector3 direction,
        Vector3 preferredWidthAxis);

void DrawFpsMuzzleFlash(
        const FpsWeaponFiringRuntimeState& firing,
        const Camera3D& viewmodelCamera);

} // namespace game
