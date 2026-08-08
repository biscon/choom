#pragma once

#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"

#include <raylib.h>

#include <array>

namespace game {

struct SectorEditorPreviewCrosshairSegmentLayout {
    Rectangle outline = {};
    Rectangle inner = {};
};

struct SectorEditorPreviewCrosshairLayout {
    Vector2 center = {};
    std::array<SectorEditorPreviewCrosshairSegmentLayout, 4> segments{};
};

struct SectorEditorPreviewHudContext {
    bool preview3DActive = false;
    Rectangle playableViewport = {};
    const FpsWeaponRegistry& weaponRegistry;
    const FpsViewmodelRuntimeState& viewmodel;
};

bool ShouldDrawSectorEditorPreviewCrosshair(
        const SectorEditorPreviewHudContext& context);
float SectorEditorPreviewHudScale(Rectangle playableViewport);
SectorEditorPreviewCrosshairLayout BuildSectorEditorPreviewCrosshairLayout(
        const FpsWeaponCrosshairDefinition& crosshair,
        Rectangle playableViewport,
        float uiScale);
void DrawSectorEditorPreviewHud(const SectorEditorPreviewHudContext& context);

} // namespace game
