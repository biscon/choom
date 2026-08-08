#pragma once

#include "game/FpsViewmodel.h"

#include <raylib.h>

namespace game {

struct SectorEditorPreviewMuzzleFlashTemporalState {
    float normalizedAge = 0.0f;
    float expansionScale = 1.0f;
    float opacity = 1.0f;
    float warmth = 0.0f;
};

struct SectorEditorPreviewMuzzleFlashRibbonAxes {
    bool valid = false;
    Vector3 first{};
    Vector3 second{};
};

SectorEditorPreviewMuzzleFlashTemporalState
EvaluateSectorEditorPreviewMuzzleFlashTemporalState(
        float ageSeconds,
        float lifetimeSeconds);

Color EvaluateSectorEditorPreviewMuzzleFlashGradient(
        const FpsMuzzleFlashRuntimeState& flash,
        float normalizedRadius,
        float opacity,
        float warmth = 0.0f);

SectorEditorPreviewMuzzleFlashRibbonAxes
BuildSectorEditorPreviewMuzzleFlashRibbonAxes(
        Vector3 direction,
        Vector3 preferredWidthAxis);

void DrawSectorEditorPreviewMuzzleFlash(
        const FpsWeaponFiringRuntimeState& firing,
        const Camera3D& viewmodelCamera);

} // namespace game
