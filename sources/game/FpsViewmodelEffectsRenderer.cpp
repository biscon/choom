#include "game/FpsViewmodelEffectsRenderer.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr int RibbonBandCount = 8;
constexpr float HotStop = 0.22f;
constexpr float WarmStop = 0.58f;
constexpr float EdgeStop = 0.82f;

float Smoothstep(float edge0, float edge1, float value)
{
    const float width = edge1 - edge0;
    if (!(width > 0.0f)) return value >= edge1 ? 1.0f : 0.0f;
    const float t = std::clamp((value - edge0) / width, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Color MixColor(Color a, Color b, float amount)
{
    const auto channel = [amount](unsigned char first, unsigned char second) {
        return static_cast<unsigned char>(std::lround(
                static_cast<float>(first)
                + (static_cast<float>(second) - static_cast<float>(first))
                        * amount));
    };
    return Color{
            channel(a.r, b.r), channel(a.g, b.g),
            channel(a.b, b.b), channel(a.a, b.a)};
}

Color GradientColor(
        const FpsMuzzleFlashRuntimeState& flash,
        float normalizedRadius)
{
    if (normalizedRadius <= HotStop) {
        return MixColor(
                flash.coreColor,
                flash.hotColor,
                normalizedRadius / HotStop);
    }
    if (normalizedRadius <= WarmStop) {
        return MixColor(
                flash.hotColor,
                flash.warmColor,
                (normalizedRadius - HotStop) / (WarmStop - HotStop));
    }
    if (normalizedRadius <= EdgeStop) {
        return MixColor(
                flash.warmColor,
                flash.edgeColor,
                (normalizedRadius - WarmStop) / (EdgeStop - WarmStop));
    }
    return flash.edgeColor;
}

void EmitVertex(Vector3 position, Color color)
{
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(position.x, position.y, position.z);
}

Color WithAlpha(Color color, float multiplier)
{
    color.a = static_cast<unsigned char>(std::lround(
            static_cast<float>(color.a)
                    * std::clamp(multiplier, 0.0f, 1.0f)));
    return color;
}

float RibbonWidthProfile(float normalizedLength)
{
    const float t = std::clamp(normalizedLength, 0.0f, 1.0f);
    return 0.18f + 0.82f * std::pow(std::sin(PI * t), 0.75f);
}

void EmitRibbon(
        const FpsMuzzleFlashRuntimeState& flash,
        const FpsMuzzleFlashTemporalState& temporal,
        Vector3 origin,
        Vector3 direction,
        Vector3 widthAxis,
        float length,
        float halfWidth)
{
    direction = Vector3Normalize(direction);
    widthAxis = Vector3Normalize(widthAxis);
    if (Vector3LengthSqr(direction) <= 0.000001f
            || Vector3LengthSqr(widthAxis) <= 0.000001f
            || !(length > 0.0f) || !(halfWidth > 0.0f)) {
        return;
    }
    for (int band = 0; band < RibbonBandCount - 1; ++band) {
        const float firstT = static_cast<float>(band)
                / static_cast<float>(RibbonBandCount);
        const float secondT = static_cast<float>(band + 1)
                / static_cast<float>(RibbonBandCount);
        const Vector3 firstCenter = Vector3Add(
                origin, Vector3Scale(direction, length * firstT));
        const Vector3 secondCenter = Vector3Add(
                origin, Vector3Scale(direction, length * secondT));
        const float firstWidth = halfWidth * RibbonWidthProfile(firstT);
        const float secondWidth = halfWidth * RibbonWidthProfile(secondT);
        const Vector3 firstLeft = Vector3Subtract(
                firstCenter, Vector3Scale(widthAxis, firstWidth));
        const Vector3 firstRight = Vector3Add(
                firstCenter, Vector3Scale(widthAxis, firstWidth));
        const Vector3 secondLeft = Vector3Subtract(
                secondCenter, Vector3Scale(widthAxis, secondWidth));
        const Vector3 secondRight = Vector3Add(
                secondCenter, Vector3Scale(widthAxis, secondWidth));
        const Color firstCenterColor =
                EvaluateFpsMuzzleFlashGradient(
                        flash, firstT, temporal.opacity, temporal.warmth);
        const Color secondCenterColor =
                EvaluateFpsMuzzleFlashGradient(
                        flash, secondT, temporal.opacity, temporal.warmth);
        const Color firstEdgeColor = WithAlpha(firstCenterColor, 0.0f);
        const Color secondEdgeColor = WithAlpha(secondCenterColor, 0.0f);

        EmitVertex(firstCenter, firstCenterColor);
        EmitVertex(firstLeft, firstEdgeColor);
        EmitVertex(secondLeft, secondEdgeColor);
        EmitVertex(firstCenter, firstCenterColor);
        EmitVertex(secondLeft, secondEdgeColor);
        EmitVertex(secondCenter, secondCenterColor);
        EmitVertex(firstCenter, firstCenterColor);
        EmitVertex(secondCenter, secondCenterColor);
        EmitVertex(secondRight, secondEdgeColor);
        EmitVertex(firstCenter, firstCenterColor);
        EmitVertex(secondRight, secondEdgeColor);
        EmitVertex(firstRight, firstEdgeColor);
    }

    const float baseT = static_cast<float>(RibbonBandCount - 1)
            / static_cast<float>(RibbonBandCount);
    const Vector3 baseCenter = Vector3Add(
            origin, Vector3Scale(direction, length * baseT));
    const float baseWidth = halfWidth * RibbonWidthProfile(baseT);
    const Vector3 baseLeft = Vector3Subtract(
            baseCenter, Vector3Scale(widthAxis, baseWidth));
    const Vector3 baseRight = Vector3Add(
            baseCenter, Vector3Scale(widthAxis, baseWidth));
    const Vector3 tip = Vector3Add(origin, Vector3Scale(direction, length));
    const Color baseColor = EvaluateFpsMuzzleFlashGradient(
            flash, baseT, temporal.opacity, temporal.warmth);
    const Color tipColor = EvaluateFpsMuzzleFlashGradient(
            flash, 1.0f, temporal.opacity, temporal.warmth);
    EmitVertex(baseLeft, WithAlpha(baseColor, 0.0f));
    EmitVertex(baseCenter, baseColor);
    EmitVertex(tip, tipColor);
    EmitVertex(baseCenter, baseColor);
    EmitVertex(baseRight, WithAlpha(baseColor, 0.0f));
    EmitVertex(tip, tipColor);
}

void EmitCrossedRibbon(
        const FpsMuzzleFlashRuntimeState& flash,
        const FpsMuzzleFlashTemporalState& temporal,
        Vector3 origin,
        Vector3 direction,
        Vector3 preferredWidthAxis,
        float length,
        float halfWidth)
{
    const FpsMuzzleFlashRibbonAxes axes =
            BuildFpsMuzzleFlashRibbonAxes(
                    direction, preferredWidthAxis);
    if (!axes.valid) return;
    EmitRibbon(
            flash, temporal, origin, direction, axes.first,
            length, halfWidth);
    EmitRibbon(
            flash, temporal, origin, direction, axes.second,
            length, halfWidth);
}

} // namespace

FpsMuzzleFlashTemporalState
EvaluateFpsMuzzleFlashTemporalState(
        float ageSeconds,
        float lifetimeSeconds)
{
    FpsMuzzleFlashTemporalState result;
    if (!(lifetimeSeconds > 0.0f) || !std::isfinite(ageSeconds)) {
        result.normalizedAge = 1.0f;
        result.expansionScale = 1.14f;
        result.opacity = 0.0f;
        result.warmth = 1.0f;
        return result;
    }
    result.normalizedAge = std::clamp(
            ageSeconds / lifetimeSeconds, 0.0f, 1.0f);
    if (result.normalizedAge <= 0.25f) return result;
    const float tail = std::clamp(
            (result.normalizedAge - 0.25f) / 0.75f, 0.0f, 1.0f);
    const float smoothTail = Smoothstep(0.0f, 1.0f, tail);
    result.expansionScale = 1.0f + 0.14f * smoothTail;
    result.opacity = (1.0f - tail) * (1.0f - tail);
    result.warmth = smoothTail;
    return result;
}

Color EvaluateFpsMuzzleFlashGradient(
        const FpsMuzzleFlashRuntimeState& flash,
        float normalizedRadius,
        float opacity,
        float warmth)
{
    const float radius = std::clamp(normalizedRadius, 0.0f, 1.0f);
    const float life = std::clamp(opacity, 0.0f, 1.0f);
    const float warmRadius = std::clamp(
            radius + 0.14f * std::clamp(warmth, 0.0f, 1.0f),
            0.0f,
            1.0f);
    const float softness = std::clamp(flash.edgeSoftness, 0.01f, 1.0f);
    const float edgeFade = 1.0f - Smoothstep(1.0f - softness, 1.0f, radius);
    Color result = GradientColor(flash, warmRadius);
    result.a = static_cast<unsigned char>(std::lround(
            static_cast<float>(result.a) * edgeFade * life));
    return result;
}

FpsMuzzleFlashRibbonAxes
BuildFpsMuzzleFlashRibbonAxes(
        Vector3 direction,
        Vector3 preferredWidthAxis)
{
    FpsMuzzleFlashRibbonAxes result;
    if (Vector3LengthSqr(direction) <= 0.000001f) return result;
    direction = Vector3Normalize(direction);

    Vector3 first = Vector3Subtract(
            preferredWidthAxis,
            Vector3Scale(
                    direction,
                    Vector3DotProduct(preferredWidthAxis, direction)));
    if (Vector3LengthSqr(first) <= 0.000001f) {
        const Vector3 fallback = std::abs(direction.y) < 0.9f
                ? Vector3{0.0f, 1.0f, 0.0f}
                : Vector3{1.0f, 0.0f, 0.0f};
        first = Vector3CrossProduct(direction, fallback);
    }
    if (Vector3LengthSqr(first) <= 0.000001f) return result;
    first = Vector3Normalize(first);
    const Vector3 second = Vector3Normalize(
            Vector3CrossProduct(direction, first));
    if (Vector3LengthSqr(second) <= 0.000001f) return result;

    result.valid = true;
    result.first = first;
    result.second = second;
    return result;
}

void DrawFpsMuzzleFlash(
        const FpsWeaponFiringRuntimeState& firing,
        const Camera3D& viewmodelCamera)
{
    if (!firing.flash.active || !firing.emission.valid
            || !(firing.flash.sizeWorld > 0.0f)
            || !(firing.flash.lifetimeSeconds > 0.0f)) {
        return;
    }
    const FpsMuzzleFlashTemporalState temporal =
            EvaluateFpsMuzzleFlashTemporalState(
                    firing.flash.ageSeconds,
                    firing.flash.lifetimeSeconds);
    if (temporal.opacity <= 0.0f) return;

    const Matrix emissionTransform = ResolveFpsMuzzleEmissionTransform(
            firing.emission, viewmodelCamera);
    const Vector3 origin = Vector3Transform(
            Vector3{}, emissionTransform);
    Vector3 right = Vector3Subtract(
            Vector3Transform(
                    Vector3{1.0f, 0.0f, 0.0f},
                    emissionTransform),
            origin);
    Vector3 up = Vector3Subtract(
            Vector3Transform(
                    Vector3{0.0f, 1.0f, 0.0f},
                    emissionTransform),
            origin);
    Vector3 forward = Vector3Subtract(
            Vector3Transform(
                    Vector3{0.0f, 0.0f, 1.0f},
                    emissionTransform),
            origin);
    right = Vector3Normalize(right);
    up = Vector3Normalize(up);
    forward = Vector3Normalize(forward);
    const float radians = firing.flash.shape.phaseRadians;
    const Vector3 rotatedRight = Vector3Add(
            Vector3Scale(right, std::cos(radians)),
            Vector3Scale(up, std::sin(radians)));
    const Vector3 rotatedUp = Vector3Add(
            Vector3Scale(right, -std::sin(radians)),
            Vector3Scale(up, std::cos(radians)));
    const float size = firing.flash.sizeWorld
            * firing.flash.shape.overallScale
            * temporal.expansionScale;

    BeginMode3D(viewmodelCamera);
    BeginBlendMode(BLEND_ADDITIVE);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    const float dominantLength = size
            * firing.flash.shape.dominantLengthScale;
    const float dominantWidth = size * 0.26f
            * firing.flash.shape.dominantWidthScale;
    EmitCrossedRibbon(
            firing.flash, temporal, origin, forward, rotatedRight,
            dominantLength, dominantWidth);
    for (int index = 1; index < firing.flash.shape.lobeCount; ++index) {
        const FpsMuzzleFlashLobe& lobe =
                firing.flash.shape.lobes[static_cast<size_t>(index)];
        const Vector3 radial = Vector3Add(
                Vector3Scale(right, std::cos(lobe.azimuthRadians)),
                Vector3Scale(up, std::sin(lobe.azimuthRadians)));
        const Vector3 tangent = Vector3Add(
                Vector3Scale(right, -std::sin(lobe.azimuthRadians)),
                Vector3Scale(up, std::cos(lobe.azimuthRadians)));
        const Vector3 direction = Vector3Normalize(Vector3Add(
                radial,
                Vector3Scale(forward, lobe.forwardComponent)));
        EmitCrossedRibbon(
                firing.flash, temporal, origin, direction, tangent,
                size * lobe.lengthScale,
                size * 0.18f * lobe.widthScale);
    }
    rlEnd();
    // rlgl defers these immediate-mode triangles until the active batch is
    // flushed. Draw them before restoring the depth mask and culling state so
    // transparent ribbon edges cannot write depth and cut holes in later
    // overlapping lobes.
    rlDrawRenderBatchActive();
    EndBlendMode();
    rlColor4ub(255, 255, 255, 255);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndMode3D();
}

} // namespace game
