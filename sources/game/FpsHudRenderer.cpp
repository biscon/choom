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
            || (!IsFpsViewmodelReadyForUse(context.viewmodel)
                && !IsFpsWeaponReloading(context.viewmodel))) {
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

FpsVitalsLayout BuildFpsVitalsLayout(
        Rectangle playableViewport,
        float uiScale,
        int fontPixelSize,
        bool includeStamina)
{
    const int margin = ScaledPixels(22.0f, uiScale);
    const int width = ScaledPixels(180.0f, uiScale);
    const int height = ScaledPixels(10.0f, uiScale);
    const int gap = ScaledPixels(8.0f, uiScale);
    const int textGap = ScaledPixels(3.0f, uiScale);
    const int x = static_cast<int>(std::lround(playableViewport.x)) + margin;
    const int bottomY = static_cast<int>(std::lround(
            playableViewport.y + playableViewport.height))
            - margin - height;

    FpsVitalsLayout result;
    if (includeStamina) {
        result.stamina.border = Rect(x, bottomY, width, height);
        result.stamina.textPosition = Vector2{
                result.stamina.border.x,
                result.stamina.border.y
                        - static_cast<float>(fontPixelSize + textGap)};
        const int healthY = static_cast<int>(std::lround(
                result.stamina.textPosition.y)) - gap - height;
        result.health.border = Rect(x, healthY, width, height);
    } else {
        result.health.border = Rect(x, bottomY, width, height);
    }
    result.health.textPosition = Vector2{
            result.health.border.x,
            result.health.border.y
                    - static_cast<float>(fontPixelSize + textGap)};
    return result;
}

FpsReloadIndicatorLayout BuildFpsReloadIndicatorLayout(
        const FpsCrosshairLayout& crosshair,
        float uiScale,
        int fontPixelSize)
{
    float extent = 0.0f;
    for (const FpsCrosshairSegmentLayout& segment : crosshair.segments) {
        extent = std::max(extent, std::abs(segment.outline.x - crosshair.center.x));
        extent = std::max(extent, std::abs(
                segment.outline.x + segment.outline.width - crosshair.center.x));
        extent = std::max(extent, std::abs(segment.outline.y - crosshair.center.y));
        extent = std::max(extent, std::abs(
                segment.outline.y + segment.outline.height - crosshair.center.y));
    }
    const float margin = static_cast<float>(ScaledPixels(8.0f, uiScale));
    const float thickness = static_cast<float>(ScaledPixels(3.0f, uiScale));
    FpsReloadIndicatorLayout result;
    result.center = crosshair.center;
    result.innerRadius = extent + margin;
    result.outerRadius = result.innerRadius + thickness;
    result.textPosition = Vector2{
            crosshair.center.x,
            crosshair.center.y + result.outerRadius
                    + static_cast<float>(ScaledPixels(8.0f, uiScale))};
    (void)fontPixelSize;
    return result;
}

Vector2 BuildFpsAmmoCounterPosition(
        const FpsVitalsLayout& vitals,
        float uiScale,
        int fontPixelSize,
        bool includeStamina)
{
    const float topTextY = includeStamina
            ? std::min(vitals.health.textPosition.y, vitals.stamina.textPosition.y)
            : vitals.health.textPosition.y;
    return Vector2{
            vitals.health.border.x,
            topTextY - static_cast<float>(fontPixelSize)
                    - static_cast<float>(ScaledPixels(8.0f, uiScale))};
}

void DrawFpsHud(const FpsHudContext& context)
{
    if (context.playableViewport.width <= 0.0f
            || context.playableViewport.height <= 0.0f) {
        return;
    }
    const float uiScale = FpsHudScale(context.playableViewport);
    const FpsWeaponDefinition* activeWeapon = FindFpsWeaponDefinition(
            context.weaponRegistry,
            context.viewmodel.activeWeaponId);
    FpsCrosshairLayout crosshairLayout;
    const bool crosshairLayoutValid = activeWeapon != nullptr;
    if (crosshairLayoutValid) {
        crosshairLayout = BuildFpsCrosshairLayout(
                activeWeapon->crosshair,
                context.playableViewport,
                uiScale);
    }
    if (ShouldDrawFpsCrosshair(context) && activeWeapon != nullptr) {
        for (const FpsCrosshairSegmentLayout& segment : crosshairLayout.segments) {
            DrawRectangleRec(segment.outline, activeWeapon->crosshair.outlineColor);
        }
        for (const FpsCrosshairSegmentLayout& segment : crosshairLayout.segments) {
            DrawRectangleRec(segment.inner, activeWeapon->crosshair.innerColor);
        }
    }

    if (activeWeapon != nullptr && crosshairLayoutValid
            && IsFpsWeaponReloading(context.viewmodel)) {
        const int fontPixelSize = context.font != nullptr
                ? context.font->pixelSize : 20;
        const FpsReloadIndicatorLayout reloadLayout =
                BuildFpsReloadIndicatorLayout(
                        crosshairLayout, uiScale, fontPixelSize);
        Color background = activeWeapon->crosshair.outlineColor;
        background.a = static_cast<unsigned char>(std::min(
                140, static_cast<int>(background.a)));
        DrawRing(
                reloadLayout.center,
                reloadLayout.innerRadius,
                reloadLayout.outerRadius,
                -90.0f,
                270.0f,
                64,
                background);
        DrawRing(
                reloadLayout.center,
                reloadLayout.innerRadius,
                reloadLayout.outerRadius,
                -90.0f,
                -90.0f + 360.0f * FpsWeaponReloadProgress(context.viewmodel),
                64,
                activeWeapon->crosshair.innerColor);
        if (context.font != nullptr) {
            constexpr const char* ReloadingText = "RELOADING";
            const float pulse = 0.5f + 0.5f * std::sin(
                    context.viewmodel.reload.totalElapsedSeconds * 10.0f);
            const Vector2 size = MeasureTextEx(
                    context.font->font,
                    ReloadingText,
                    static_cast<float>(context.font->pixelSize),
                    1.0f);
            DrawTextEx(
                    context.font->font,
                    ReloadingText,
                    Vector2{
                            reloadLayout.textPosition.x - size.x * 0.5f,
                            reloadLayout.textPosition.y},
                    static_cast<float>(context.font->pixelSize),
                    1.0f,
                    Color{235, 235, 225,
                            static_cast<unsigned char>(150.0f + pulse * 105.0f)});
        }
    }

    if (context.health == nullptr) return;
    const engine::FontAsset* fontAsset = context.font;
    if (fontAsset == nullptr) return;
    const FpsVitalsLayout vitals = BuildFpsVitalsLayout(
            context.playableViewport,
            uiScale,
            fontAsset->pixelSize,
            context.stamina != nullptr);
    if (context.showAmmo && activeWeapon != nullptr) {
        char ammoText[64] = {};
        std::snprintf(
                ammoText,
                sizeof(ammoText),
                "%d / %llu",
                std::max(0, context.loadedRounds),
                static_cast<unsigned long long>(context.reserveRounds));
        DrawTextEx(
                fontAsset->font,
                ammoText,
                BuildFpsAmmoCounterPosition(
                        vitals,
                        uiScale,
                        fontAsset->pixelSize,
                        context.stamina != nullptr),
                static_cast<float>(fontAsset->pixelSize),
                1.0f,
                Color{210, 210, 202, 190});
    }
    const Rectangle healthBorder = vitals.health.border;
    DrawRectangleRec(healthBorder, Color{8, 10, 12, 210});
    const Rectangle healthInterior = Inset(healthBorder, 2);
    const float ratio = context.health->maximum > 0
            ? std::clamp(
                    static_cast<float>(context.health->current)
                            / static_cast<float>(context.health->maximum),
                    0.0f,
                    1.0f)
            : 0.0f;
    DrawRectangleRec(
            Rectangle{
                    healthInterior.x,
                    healthInterior.y,
                    healthInterior.width * ratio,
                    healthInterior.height},
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
            vitals.health.textPosition,
            static_cast<float>(fontAsset->pixelSize),
            1.0f,
            Color{235, 235, 225, 235});

    if (context.stamina == nullptr) return;
    const Rectangle staminaBorder = vitals.stamina.border;
    DrawRectangleRec(staminaBorder, Color{8, 10, 12, 210});
    const Rectangle staminaInterior = Inset(staminaBorder, 2);
    DrawRectangleRec(
            Rectangle{
                    staminaInterior.x,
                    staminaInterior.y,
                    staminaInterior.width * PlayerStaminaRatio(*context.stamina),
                    staminaInterior.height},
            Color{45, 100, 190, 235});
    char staminaText[48] = {};
    std::snprintf(
            staminaText,
            sizeof(staminaText),
            "%.0f / %.0f",
            context.stamina->current,
            context.stamina->maximum);
    DrawTextEx(
            fontAsset->font,
            staminaText,
            vitals.stamina.textPosition,
            static_cast<float>(fontAsset->pixelSize),
            1.0f,
            Color{235, 235, 225, 235});
}

} // namespace game
