#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

namespace game {

struct SectorVolumetricMediumSample {
    float extinction = 0.0f;
    Vector3 extinctionWeightedTint = {};
    Vector3 extinctionWeightedStaticRadiance = {};
};

struct SectorVolumetricIntegrationState {
    Vector3 premultipliedRadiance = {};
    float transmittance = 1.0f;
};

SectorVolumetricMediumSample CombineSectorVolumetricMediumSamples(
        const SectorVolumetricMediumSample& left,
        const SectorVolumetricMediumSample& right);
float EvaluateSectorHenyeyGreensteinPhase(float cosineTheta, float anisotropy);
void IntegrateSectorVolumetricStep(
        SectorVolumetricIntegrationState& state,
        float opticalDepth,
        Vector3 radiancePerUnitExtinction);
Vector4 FinishSectorVolumetricIntegration(
        const SectorVolumetricIntegrationState& state);
float ResolveSectorVolumetricSceneDistance(
        float depthSample,
        float nearPlane,
        float farPlane,
        float rayForwardDot,
        float maximumDistanceWorld);
float ComputeSectorFogOpticalDepth(
        const SectorTopologyFogSettings& settings,
        Vector3 cameraPosition,
        Vector3 worldPosition,
        float pathDistanceWorld);
float ComputeSectorAnalyticFogTailOpacity(
        const SectorTopologyFogSettings& settings,
        Vector3 cameraPosition,
        Vector3 worldPosition,
        float volumetricMaximumDistanceWorld);

} // namespace game
