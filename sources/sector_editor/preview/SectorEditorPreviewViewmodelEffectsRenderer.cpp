#include "sector_editor/preview/SectorEditorPreviewViewmodelEffectsRenderer.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr int AngularPointCount = 16;
constexpr int RadialBandCount = 12;
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

Vector3 StarPoint(
        Vector3 origin,
        Vector3 right,
        Vector3 up,
        float radius,
        float normalizedRadius,
        int angularIndex)
{
    const float angle = 2.0f * PI * static_cast<float>(angularIndex)
            / static_cast<float>(AngularPointCount);
    const float boundaryScale = (angularIndex & 1) == 0 ? 1.35f : 0.72f;
    return Vector3Add(origin, Vector3Add(
            Vector3Scale(
                    right,
                    std::cos(angle) * radius * normalizedRadius
                            * boundaryScale),
            Vector3Scale(
                    up,
                    std::sin(angle) * radius * normalizedRadius
                            * boundaryScale)));
}

} // namespace

Color EvaluateSectorEditorPreviewMuzzleFlashGradient(
        const FpsMuzzleFlashRuntimeState& flash,
        float normalizedRadius,
        float lifeAmount)
{
    const float radius = std::clamp(normalizedRadius, 0.0f, 1.0f);
    const float life = std::clamp(lifeAmount, 0.0f, 1.0f);
    const float softness = std::clamp(flash.edgeSoftness, 0.01f, 1.0f);
    const float edgeFade = 1.0f - Smoothstep(1.0f - softness, 1.0f, radius);
    Color result = GradientColor(flash, radius);
    result.a = static_cast<unsigned char>(std::lround(
            static_cast<float>(result.a) * edgeFade * life));
    return result;
}

void DrawSectorEditorPreviewMuzzleFlash(
        const FpsWeaponFiringRuntimeState& firing,
        const Camera3D& viewmodelCamera)
{
    if (!firing.flash.active || !firing.muzzleWorldTransformValid
            || !(firing.flash.sizeWorld > 0.0f)
            || !(firing.flash.lifetimeSeconds > 0.0f)) {
        return;
    }
    const float life = std::clamp(
            1.0f - firing.flash.ageSeconds / firing.flash.lifetimeSeconds,
            0.0f,
            1.0f);
    if (life <= 0.0f) return;

    const Vector3 origin = Vector3Transform(
            Vector3{}, firing.muzzleWorldTransform);
    Vector3 right = Vector3Subtract(
            Vector3Transform(
                    Vector3{1.0f, 0.0f, 0.0f},
                    firing.muzzleWorldTransform),
            origin);
    Vector3 up = Vector3Subtract(
            Vector3Transform(
                    Vector3{0.0f, 1.0f, 0.0f},
                    firing.muzzleWorldTransform),
            origin);
    right = Vector3Normalize(right);
    up = Vector3Normalize(up);
    const float radians = firing.flash.rotationDegrees * DEG2RAD;
    const Vector3 rotatedRight = Vector3Add(
            Vector3Scale(right, std::cos(radians)),
            Vector3Scale(up, std::sin(radians)));
    const Vector3 rotatedUp = Vector3Add(
            Vector3Scale(right, -std::sin(radians)),
            Vector3Scale(up, std::cos(radians)));
    const float radius = firing.flash.sizeWorld * (0.65f + 0.35f * life);

    BeginMode3D(viewmodelCamera);
    BeginBlendMode(BLEND_ADDITIVE);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int band = 0; band < RadialBandCount; ++band) {
        const float innerRadius = static_cast<float>(band)
                / static_cast<float>(RadialBandCount);
        const float outerRadius = static_cast<float>(band + 1)
                / static_cast<float>(RadialBandCount);
        const Color innerColor = EvaluateSectorEditorPreviewMuzzleFlashGradient(
                firing.flash, innerRadius, life);
        const Color outerColor = EvaluateSectorEditorPreviewMuzzleFlashGradient(
                firing.flash, outerRadius, life);
        for (int point = 0; point < AngularPointCount; ++point) {
            const int nextPoint = (point + 1) % AngularPointCount;
            const Vector3 outerA = StarPoint(
                    origin, rotatedRight, rotatedUp,
                    radius, outerRadius, point);
            const Vector3 outerB = StarPoint(
                    origin, rotatedRight, rotatedUp,
                    radius, outerRadius, nextPoint);
            if (band == 0) {
                EmitVertex(origin, innerColor);
                EmitVertex(outerA, outerColor);
                EmitVertex(outerB, outerColor);
                continue;
            }
            const Vector3 innerA = StarPoint(
                    origin, rotatedRight, rotatedUp,
                    radius, innerRadius, point);
            const Vector3 innerB = StarPoint(
                    origin, rotatedRight, rotatedUp,
                    radius, innerRadius, nextPoint);
            EmitVertex(innerA, innerColor);
            EmitVertex(outerA, outerColor);
            EmitVertex(outerB, outerColor);
            EmitVertex(innerA, innerColor);
            EmitVertex(outerB, outerColor);
            EmitVertex(innerB, innerColor);
        }
    }
    rlEnd();
    rlColor4ub(255, 255, 255, 255);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
    EndMode3D();
}

} // namespace game
