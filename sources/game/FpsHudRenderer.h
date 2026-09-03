#pragma once

#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"
#include "game/Health.h"
#include "game/PlayerStamina.h"
#include "game/PlayerOxygen.h"
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

struct FpsStatusBarLayout {
    Rectangle border = {};
    Vector2 textPosition = {};
};

struct FpsVitalsLayout {
    FpsStatusBarLayout health;
    FpsStatusBarLayout stamina;
    FpsStatusBarLayout oxygen;
};

struct FpsReloadIndicatorLayout {
    Vector2 center = {};
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
    Vector2 textPosition = {};
};

struct FpsHudContext {
    bool preview3DActive = false;
    Rectangle playableViewport = {};
    const FpsWeaponRegistry& weaponRegistry;
    const FpsViewmodelRuntimeState& viewmodel;
    const engine::FontAsset* font = nullptr;
    const Health* health = nullptr;
    const PlayerStamina* stamina = nullptr;
    const PlayerOxygen* oxygen = nullptr;
    float oxygenAlpha = 0.0f;
    int loadedRounds = 0;
    std::uint64_t reserveRounds = 0;
    bool showAmmo = false;
};

bool ShouldDrawFpsCrosshair(
        const FpsHudContext& context);
float FpsHudScale(Rectangle playableViewport);
FpsCrosshairLayout BuildFpsCrosshairLayout(
        const FpsWeaponCrosshairDefinition& crosshair,
        Rectangle playableViewport,
        float uiScale);
FpsVitalsLayout BuildFpsVitalsLayout(
        Rectangle playableViewport,
        float uiScale,
        int fontPixelSize,
        bool includeStamina,
        bool includeOxygen = false);
FpsReloadIndicatorLayout BuildFpsReloadIndicatorLayout(
        const FpsCrosshairLayout& crosshair,
        float uiScale,
        int fontPixelSize);
Vector2 BuildFpsAmmoCounterPosition(
        const FpsVitalsLayout& vitals,
        float uiScale,
        int fontPixelSize,
        bool includeStamina,
        bool includeOxygen = false);
void DrawFpsHud(const FpsHudContext& context);

} // namespace game
