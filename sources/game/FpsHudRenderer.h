#pragma once

#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"
#include "game/Health.h"
#include "engine/assets/FontAssets.h"

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
    const engine::FontAsset* font = nullptr;
    const Health* health = nullptr;
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
