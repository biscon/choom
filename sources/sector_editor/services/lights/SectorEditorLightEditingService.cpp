#include "sector_editor/services/lights/SectorEditorLightEditingService.h"

#include "sector_editor/SectorEditorDirtyState.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <utility>

namespace game {
namespace {

bool SameVector3(Vector3 lhs, Vector3 rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool SameColor(Color lhs, Color rhs)
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

template<typename Value>
bool SetValue(Value& target, Value value)
{
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

bool SetVector3(Vector3& target, Vector3 value)
{
    if (SameVector3(target, value)) {
        return false;
    }
    target = value;
    return true;
}

bool SetColorValue(Color& target, Color value)
{
    value.a = 255;
    if (SameColor(target, value)) {
        return false;
    }
    target = value;
    return true;
}

float ClampConeDegrees(float value)
{
    return std::clamp(value, 0.0f, 179.0f);
}

} // namespace

SectorEditorLightEditingService::SectorEditorLightEditingService(
        SectorEditorLightEditingServiceContext context)
    : context_(std::move(context))
{
}

void SectorEditorLightEditingService::MarkEdited(const char* status)
{
    MarkSectorEditorTopologyDocumentEdited(context_.state, context_.statusText, status);
}

bool SectorEditorLightEditingService::SetStaticLightPosition(
        SectorTopologyStaticPointLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightIntensity(
        SectorTopologyStaticPointLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightRadius(
        SectorTopologyStaticPointLight& light,
        float radius)
{
    radius = ClampLightRadius(radius);
    if (!SetValue(light.radius, radius)) {
        return false;
    }
    light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.radius);
    MarkEdited(TextFormat("Updated static light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightSourceRadius(
        SectorTopologyStaticPointLight& light,
        float sourceRadius)
{
    sourceRadius = ClampLightSourceRadius(sourceRadius, light.radius);
    if (!SetValue(light.sourceRadius, sourceRadius)) {
        return false;
    }
    MarkEdited("Updated light source radius");
    return true;
}

bool SectorEditorLightEditingService::SetStaticLightColor(
        SectorTopologyStaticPointLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static light %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightPosition(
        SectorTopologyStaticSpotLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightTarget(
        SectorTopologyStaticSpotLight& light,
        Vector3 target)
{
    if (!SetVector3(light.target, target)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightRange(
        SectorTopologyStaticSpotLight& light,
        float range)
{
    range = ClampLightRadius(range);
    if (!SetValue(light.range, range)) {
        return false;
    }
    light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.range);
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightSourceRadius(
        SectorTopologyStaticSpotLight& light,
        float sourceRadius)
{
    sourceRadius = ClampLightSourceRadius(sourceRadius, light.range);
    if (!SetValue(light.sourceRadius, sourceRadius)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d source radius", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightInnerCone(
        SectorTopologyStaticSpotLight& light,
        float innerConeDegrees)
{
    innerConeDegrees = ClampConeDegrees(innerConeDegrees);
    if (!SetValue(light.innerConeDegrees, innerConeDegrees)) {
        return false;
    }
    light.outerConeDegrees = ClampConeDegrees(std::max(light.outerConeDegrees, light.innerConeDegrees));
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightOuterCone(
        SectorTopologyStaticSpotLight& light,
        float outerConeDegrees)
{
    outerConeDegrees = ClampConeDegrees(std::max(outerConeDegrees, light.innerConeDegrees));
    if (!SetValue(light.outerConeDegrees, outerConeDegrees)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightIntensity(
        SectorTopologyStaticSpotLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetStaticSpotLightColor(
        SectorTopologyStaticSpotLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated static spot %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightEnabled(
        SectorTopologyDynamicPointLight& light,
        bool enabled)
{
    if (!SetValue(light.enabled, enabled)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d enabled", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlicker(
        SectorTopologyDynamicPointLight& light,
        bool flicker)
{
    if (!SetValue(light.flicker, flicker)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d flicker", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlickerSpeed(
        SectorTopologyDynamicPointLight& light,
        float flickerSpeed)
{
    flickerSpeed = ClampDynamicLightFlickerSpeed(flickerSpeed);
    if (!SetValue(light.flickerSpeed, flickerSpeed)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightFlickerAmount(
        SectorTopologyDynamicPointLight& light,
        float flickerAmount)
{
    flickerAmount = ClampDynamicLightFlickerAmount(flickerAmount);
    if (!SetValue(light.flickerAmount, flickerAmount)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightPosition(
        SectorTopologyDynamicPointLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightIntensity(
        SectorTopologyDynamicPointLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightRadius(
        SectorTopologyDynamicPointLight& light,
        float radius)
{
    radius = ClampLightRadius(radius);
    if (!SetValue(light.radius, radius)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicLightColor(
        SectorTopologyDynamicPointLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic light %d color", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightEnabled(
        SectorTopologyDynamicSpotLight& light,
        bool enabled)
{
    if (!SetValue(light.enabled, enabled)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d enabled", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlicker(
        SectorTopologyDynamicSpotLight& light,
        bool flicker)
{
    if (!SetValue(light.flicker, flicker)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d flicker", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlickerSpeed(
        SectorTopologyDynamicSpotLight& light,
        float flickerSpeed)
{
    flickerSpeed = ClampDynamicLightFlickerSpeed(flickerSpeed);
    if (!SetValue(light.flickerSpeed, flickerSpeed)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightFlickerAmount(
        SectorTopologyDynamicSpotLight& light,
        float flickerAmount)
{
    flickerAmount = ClampDynamicLightFlickerAmount(flickerAmount);
    if (!SetValue(light.flickerAmount, flickerAmount)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightCastsShadow(
        SectorTopologyDynamicSpotLight& light,
        bool castsShadow)
{
    if (!SetValue(light.castsShadow, castsShadow)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow request", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowPriority(
        SectorTopologyDynamicSpotLight& light,
        int shadowPriority)
{
    shadowPriority = ClampDynamicSpotLightShadowPriority(shadowPriority);
    if (!SetValue(light.shadowPriority, shadowPriority)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow priority", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowBias(
        SectorTopologyDynamicSpotLight& light,
        float shadowBias)
{
    shadowBias = ClampDynamicSpotLightShadowBias(shadowBias);
    if (!SetValue(light.shadowBias, shadowBias)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow bias", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowStrength(
        SectorTopologyDynamicSpotLight& light,
        float shadowStrength)
{
    shadowStrength = ClampDynamicSpotLightShadowStrength(shadowStrength);
    if (!SetValue(light.shadowStrength, shadowStrength)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow strength", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightShadowSoftness(
        SectorTopologyDynamicSpotLight& light,
        float shadowSoftness)
{
    shadowSoftness = ClampDynamicSpotLightShadowSoftness(shadowSoftness);
    if (!SetValue(light.shadowSoftness, shadowSoftness)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d shadow softness", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightPosition(
        SectorTopologyDynamicSpotLight& light,
        Vector3 position)
{
    if (!SetVector3(light.position, position)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightTarget(
        SectorTopologyDynamicSpotLight& light,
        Vector3 target)
{
    if (!SetVector3(light.target, target)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightIntensity(
        SectorTopologyDynamicSpotLight& light,
        float intensity)
{
    intensity = ClampLightIntensity(intensity);
    if (!SetValue(light.intensity, intensity)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightRange(
        SectorTopologyDynamicSpotLight& light,
        float range)
{
    range = ClampLightRadius(range);
    if (!SetValue(light.range, range)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightInnerCone(
        SectorTopologyDynamicSpotLight& light,
        float innerConeDegrees)
{
    innerConeDegrees = ClampConeDegrees(innerConeDegrees);
    if (!SetValue(light.innerConeDegrees, innerConeDegrees)) {
        return false;
    }
    light.outerConeDegrees = ClampConeDegrees(std::max(light.outerConeDegrees, light.innerConeDegrees));
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightOuterCone(
        SectorTopologyDynamicSpotLight& light,
        float outerConeDegrees)
{
    outerConeDegrees = ClampConeDegrees(std::max(outerConeDegrees, light.innerConeDegrees));
    if (!SetValue(light.outerConeDegrees, outerConeDegrees)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d", light.id));
    return true;
}

bool SectorEditorLightEditingService::SetDynamicSpotLightColor(
        SectorTopologyDynamicSpotLight& light,
        Color color)
{
    if (!SetColorValue(light.color, color)) {
        return false;
    }
    MarkEdited(TextFormat("Updated dynamic spot %d color", light.id));
    return true;
}

} // namespace game
