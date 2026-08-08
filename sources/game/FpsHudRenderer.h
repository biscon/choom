#pragma once

#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"

#include <raylib.h>

#include <array>

namespace game {

struct FpsCrosshairSegmentLayout {
    Rectangle outline = {};
    Rectangle inner = {};
};

struct FpsCrosshairLayout {
    Vector2 center = {};
    std::array<FpsCrosshairSegmentLayout, 4> segments{};
};

struct FpsHudContext {
    bool preview3DActive = false;
    Rectangle playableViewport = {};
    const FpsWeaponRegistry& weaponRegistry;
    const FpsViewmodelRuntimeState& viewmodel;
};

bool ShouldDrawFpsCrosshair(
        const FpsHudContext& context);
float FpsHudScale(Rectangle playableViewport);
FpsCrosshairLayout BuildFpsCrosshairLayout(
        const FpsWeaponCrosshairDefinition& crosshair,
        Rectangle playableViewport,
        float uiScale);
void DrawFpsHud(const FpsHudContext& context);

} // namespace game
