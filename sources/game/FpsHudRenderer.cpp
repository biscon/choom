#include "game/FpsHudRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {
namespace {

constexpr size_t LeftSegment = 0;
constexpr size_t RightSegment = 1;
constexpr size_t TopSegment = 2;
constexpr size_t BottomSegment = 3;
constexpr float ReferenceWidth = 1920.0f;
constexpr float ReferenceHeight = 1080.0f;

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

bool ShouldDrawFpsCrosshair(
        const FpsHudContext& context)
{
    if (!context.preview3DActive
            || context.viewmodel.activeWeaponId.empty()
            || !IsFpsViewmodelReadyForUse(context.viewmodel)) {
        return false;
    }
    const FpsWeaponDefinition* weapon = FindFpsWeaponDefinition(
            context.weaponRegistry,
            context.viewmodel.activeWeaponId);
    return weapon != nullptr && weapon->crosshair.enabled;
}

float FpsHudScale(Rectangle playableViewport)
{
    if (!std::isfinite(playableViewport.width)
            || !std::isfinite(playableViewport.height)
            || playableViewport.width <= 0.0f
            || playableViewport.height <= 0.0f) {
        return 1.0f;
    }
    return std::min(
            playableViewport.width / ReferenceWidth,
            playableViewport.height / ReferenceHeight);
}

FpsCrosshairLayout BuildFpsCrosshairLayout(
        const FpsWeaponCrosshairDefinition& crosshair,
        Rectangle playableViewport,
        float uiScale)
{
    FpsCrosshairLayout result;
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

    for (FpsCrosshairSegmentLayout& segment : result.segments) {
        segment.inner = Inset(segment.outline, outlineThickness);
    }
    return result;
}

void DrawFpsHud(const FpsHudContext& context)
{
    if (context.playableViewport.width <= 0.0f
            || context.playableViewport.height <= 0.0f) {
        return;
    }
    const float uiScale = FpsHudScale(context.playableViewport);
    if (ShouldDrawFpsCrosshair(context)) {
        const FpsWeaponDefinition* weapon = FindFpsWeaponDefinition(
                context.weaponRegistry,
                context.viewmodel.activeWeaponId);
        if (weapon != nullptr) {
            const FpsCrosshairLayout layout = BuildFpsCrosshairLayout(
                    weapon->crosshair,
                    context.playableViewport,
                    uiScale);
            for (const FpsCrosshairSegmentLayout& segment : layout.segments) {
                DrawRectangleRec(segment.outline, weapon->crosshair.outlineColor);
            }
            for (const FpsCrosshairSegmentLayout& segment : layout.segments) {
                DrawRectangleRec(segment.inner, weapon->crosshair.innerColor);
            }
        }
    }

    if (context.health == nullptr) return;
    const engine::FontAsset* fontAsset = context.font;
    if (fontAsset == nullptr) return;
    const int margin = ScaledPixels(22.0f, uiScale);
    const int width = ScaledPixels(180.0f, uiScale);
    const int height = ScaledPixels(10.0f, uiScale);
    const int x = static_cast<int>(std::lround(context.playableViewport.x)) + margin;
    const int y = static_cast<int>(std::lround(
            context.playableViewport.y + context.playableViewport.height))
            - margin - height;
    const Rectangle border = Rect(x, y, width, height);
    DrawRectangleRec(border, Color{8, 10, 12, 210});
    const Rectangle interior = Inset(border, 2);
    const float ratio = context.health->maximum > 0
            ? std::clamp(
                    static_cast<float>(context.health->current)
                            / static_cast<float>(context.health->maximum),
                    0.0f,
                    1.0f)
            : 0.0f;
    DrawRectangleRec(
            Rectangle{interior.x, interior.y, interior.width * ratio, interior.height},
            Color{145, 38, 37, 235});
    char healthText[48] = {};
    std::snprintf(
            healthText,
            sizeof(healthText),
            "%d / %d",
            context.health->current,
            context.health->maximum);
    DrawTextEx(
            fontAsset->font,
            healthText,
            Vector2{border.x, border.y - static_cast<float>(fontAsset->pixelSize) - 3.0f},
            static_cast<float>(fontAsset->pixelSize),
            1.0f,
            Color{235, 235, 225, 235});
}

} // namespace game
