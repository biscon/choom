#pragma once

#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"

namespace game
{

constexpr double SectorReflectionUpdateInterval = 0.1;
constexpr double SectorReflectionTransitionSeconds = 0.1;
Camera3D SectorReflectionFaceCamera(Vector3 position, int face);
struct SectorReflectionCaptureCulling {
    Camera3D camera{};
    float nearPlane = 0.01f, farPlane = 1000.0f;
    std::size_t objectsDrawn = 0, objectsCulled = 0;
};
bool AcceptSectorReflectionObject(SectorReflectionCaptureCulling* culling,
        BoundingBox bounds, bool validBounds = true);

void MarkSectorReflectionProbeDirty(SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                    bool discontinuity);
bool CanStartSectorReflectionProbe(const SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                   bool preparing, bool demanded = true);
void AdvanceSectorReflectionDemandFrame(SectorReflectionDemand& demand);
bool IsSectorReflectionProbeDemanded(const SectorPbrEnvironment& environment,
        const SectorReflectionDemand& demand, std::size_t index, bool preparing);
int SelectSectorReflectionProbeUpdate(const SectorPbrEnvironment& environment,
        const SectorReflectionDemand& demand, bool preparing, Vector3 viewerPosition,
        int activeProbeIndex);
SectorPreviewDynamicPointLightUniform NormalizeSectorReflectionLight(
        SectorPreviewDynamicPointLightUniform light);
bool SectorReflectionBoundsInView(const Camera3D& camera, float aspect, float nearPlane,
        float farPlane, BoundingBox bounds);
SectorReflectionDemand* SectorReflectionDemandForBounds(SectorReflectionDemand* demand,
        BoundingBox bounds);
void PublishSectorReflectionProbe(SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                  std::uint64_t capturedRevision);
bool SectorReflectionLightsMatch(const SectorPreviewDynamicPointLightUniform &a,
                                 const SectorPreviewDynamicPointLightUniform &b);
bool SectorReflectionLightAffectsProbe(const SectorPreviewDynamicPointLightUniform &light,
                                       const SectorCompiledReflectionProbe &probe);
bool SectorReflectionLightDiscontinuity(const SectorPreviewDynamicPointLightUniform *old,
                                        const SectorPreviewDynamicPointLightUniform *current);

} // namespace game
