#include "sector_editor/preview/SectorEditorPreviewHudRenderer.h"

#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr size_t LeftSegment = 0;
constexpr size_t RightSegment = 1;
constexpr size_t TopSegment = 2;
constexpr size_t BottomSegment = 3;

int ScaledPixels(float authoredPixels, float uiScale)
{
    return std::max(1, static_cast<int>(std::lround(
            authoredPixels * std::max(0.0f, uiScale))));
}

Rectangle Rect(int x, int y, int width, int height)
{
    return Rectangle{
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(width),
            static_cast<float>(height)};
}

Rectangle Inset(Rectangle rectangle, int pixels)
{
    const float inset = static_cast<float>(pixels);
    return Rectangle{
            rectangle.x + inset,
            rectangle.y + inset,
            rectangle.width - inset * 2.0f,
            rectangle.height - inset * 2.0f};
}

} // namespace

bool ShouldDrawSectorEditorPreviewCrosshair(
        const SectorEditorPreviewHudContext& context)
{
    if (!context.preview3DActive
            || context.viewmodel.activeWeaponId.empty()
            || context.viewmodel.holstered) {
        return false;
    }
    const FpsWeaponDefinition* weapon = FindFpsWeaponDefinition(
            context.weaponRegistry,
            context.viewmodel.activeWeaponId);
    return weapon != nullptr && weapon->crosshair.enabled;
}

float SectorEditorPreviewHudScale(Rectangle playableViewport)
{
    if (!std::isfinite(playableViewport.width)
            || !std::isfinite(playableViewport.height)
            || playableViewport.width <= 0.0f
            || playableViewport.height <= 0.0f) {
        return 1.0f;
    }
    return std::min(
            playableViewport.width / EditorWidth,
            playableViewport.height / EditorHeight);
}

SectorEditorPreviewCrosshairLayout BuildSectorEditorPreviewCrosshairLayout(
        const FpsWeaponCrosshairDefinition& crosshair,
        Rectangle playableViewport,
        float uiScale)
{
    SectorEditorPreviewCrosshairLayout result;
    const int centerX = static_cast<int>(std::lround(
            playableViewport.x + playableViewport.width * 0.5f));
    const int centerY = static_cast<int>(std::lround(
            playableViewport.y + playableViewport.height * 0.5f));
    result.center = Vector2{
            static_cast<float>(centerX),
            static_cast<float>(centerY)};

    const int gap = ScaledPixels(crosshair.centerGapPixels, uiScale);
    const int length = ScaledPixels(
            crosshair.segmentLengthPixels, uiScale);
    const int innerThickness = ScaledPixels(
            crosshair.innerThicknessPixels, uiScale);
    const int outlineThickness = ScaledPixels(
            crosshair.outlineThicknessPixels, uiScale);
    const int outlinedLength = length + outlineThickness * 2;
    const int outlinedThickness = innerThickness + outlineThickness * 2;

    const int gapLeft = centerX - gap / 2;
    const int gapRight = gapLeft + gap;
    const int horizontalY = centerY - outlinedThickness / 2;
    result.segments[LeftSegment].outline = Rect(
            gapLeft - outlinedLength,
            horizontalY,
            outlinedLength,
            outlinedThickness);
    result.segments[RightSegment].outline = Rect(
            gapRight,
            horizontalY,
            outlinedLength,
            outlinedThickness);

    const int gapTop = centerY - gap / 2;
    const int gapBottom = gapTop + gap;
    const int verticalX = centerX - outlinedThickness / 2;
    result.segments[TopSegment].outline = Rect(
            verticalX,
            gapTop - outlinedLength,
            outlinedThickness,
            outlinedLength);
    result.segments[BottomSegment].outline = Rect(
            verticalX,
            gapBottom,
            outlinedThickness,
            outlinedLength);

    for (SectorEditorPreviewCrosshairSegmentLayout& segment : result.segments) {
        segment.inner = Inset(segment.outline, outlineThickness);
    }
    return result;
}

void DrawSectorEditorPreviewHud(const SectorEditorPreviewHudContext& context)
{
    if (!ShouldDrawSectorEditorPreviewCrosshair(context)
            || context.playableViewport.width <= 0.0f
            || context.playableViewport.height <= 0.0f) {
        return;
    }
    const FpsWeaponDefinition* weapon = FindFpsWeaponDefinition(
            context.weaponRegistry,
            context.viewmodel.activeWeaponId);
    if (weapon == nullptr) return;

    const SectorEditorPreviewCrosshairLayout layout =
            BuildSectorEditorPreviewCrosshairLayout(
                    weapon->crosshair,
                    context.playableViewport,
                    SectorEditorPreviewHudScale(context.playableViewport));
    for (const SectorEditorPreviewCrosshairSegmentLayout& segment
            : layout.segments) {
        DrawRectangleRec(segment.outline, weapon->crosshair.outlineColor);
    }
    for (const SectorEditorPreviewCrosshairSegmentLayout& segment
            : layout.segments) {
        DrawRectangleRec(segment.inner, weapon->crosshair.innerColor);
    }
}

} // namespace game
