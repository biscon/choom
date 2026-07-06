#pragma once

#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <string>

namespace game {

struct SectorEditorLightEditingServiceContext {
    SectorEditorState& state;
    SectorEditorUiState& uiState;
    std::string& statusText;
};

class SectorEditorLightEditingService {
public:
    explicit SectorEditorLightEditingService(SectorEditorLightEditingServiceContext context);

    bool SetStaticLightPosition(SectorTopologyStaticPointLight& light, Vector3 position);
    bool SetStaticLightIntensity(SectorTopologyStaticPointLight& light, float intensity);
    bool SetStaticLightRadius(SectorTopologyStaticPointLight& light, float radius);
    bool SetStaticLightSourceRadius(SectorTopologyStaticPointLight& light, float sourceRadius);
    bool SetStaticLightColor(SectorTopologyStaticPointLight& light, Color color);

    bool SetStaticSpotLightPosition(SectorTopologyStaticSpotLight& light, Vector3 position);
    bool SetStaticSpotLightTarget(SectorTopologyStaticSpotLight& light, Vector3 target);
    bool SetStaticSpotLightRange(SectorTopologyStaticSpotLight& light, float range);
    bool SetStaticSpotLightSourceRadius(SectorTopologyStaticSpotLight& light, float sourceRadius);
    bool SetStaticSpotLightInnerCone(SectorTopologyStaticSpotLight& light, float innerConeDegrees);
    bool SetStaticSpotLightOuterCone(SectorTopologyStaticSpotLight& light, float outerConeDegrees);
    bool SetStaticSpotLightIntensity(SectorTopologyStaticSpotLight& light, float intensity);
    bool SetStaticSpotLightColor(SectorTopologyStaticSpotLight& light, Color color);

    bool SetDynamicLightEnabled(SectorTopologyDynamicPointLight& light, bool enabled);
    bool SetDynamicLightFlicker(SectorTopologyDynamicPointLight& light, bool flicker);
    bool SetDynamicLightFlickerSpeed(SectorTopologyDynamicPointLight& light, float flickerSpeed);
    bool SetDynamicLightFlickerAmount(SectorTopologyDynamicPointLight& light, float flickerAmount);
    bool SetDynamicLightPosition(SectorTopologyDynamicPointLight& light, Vector3 position);
    bool SetDynamicLightIntensity(SectorTopologyDynamicPointLight& light, float intensity);
    bool SetDynamicLightRadius(SectorTopologyDynamicPointLight& light, float radius);
    bool SetDynamicLightColor(SectorTopologyDynamicPointLight& light, Color color);

    bool SetDynamicSpotLightEnabled(SectorTopologyDynamicSpotLight& light, bool enabled);
    bool SetDynamicSpotLightFlicker(SectorTopologyDynamicSpotLight& light, bool flicker);
    bool SetDynamicSpotLightFlickerSpeed(SectorTopologyDynamicSpotLight& light, float flickerSpeed);
    bool SetDynamicSpotLightFlickerAmount(SectorTopologyDynamicSpotLight& light, float flickerAmount);
    bool SetDynamicSpotLightCastsShadow(SectorTopologyDynamicSpotLight& light, bool castsShadow);
    bool SetDynamicSpotLightShadowPriority(SectorTopologyDynamicSpotLight& light, int shadowPriority);
    bool SetDynamicSpotLightShadowBias(SectorTopologyDynamicSpotLight& light, float shadowBias);
    bool SetDynamicSpotLightShadowStrength(SectorTopologyDynamicSpotLight& light, float shadowStrength);
    bool SetDynamicSpotLightShadowSoftness(SectorTopologyDynamicSpotLight& light, float shadowSoftness);
    bool SetDynamicSpotLightPosition(SectorTopologyDynamicSpotLight& light, Vector3 position);
    bool SetDynamicSpotLightTarget(SectorTopologyDynamicSpotLight& light, Vector3 target);
    bool SetDynamicSpotLightIntensity(SectorTopologyDynamicSpotLight& light, float intensity);
    bool SetDynamicSpotLightRange(SectorTopologyDynamicSpotLight& light, float range);
    bool SetDynamicSpotLightInnerCone(SectorTopologyDynamicSpotLight& light, float innerConeDegrees);
    bool SetDynamicSpotLightOuterCone(SectorTopologyDynamicSpotLight& light, float outerConeDegrees);
    bool SetDynamicSpotLightColor(SectorTopologyDynamicSpotLight& light, Color color);

private:
    void MarkEdited(const char* status);

    SectorEditorLightEditingServiceContext context_;
};

} // namespace game
