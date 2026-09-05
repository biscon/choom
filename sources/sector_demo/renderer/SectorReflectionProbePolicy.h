#pragma once

#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"

namespace game
{

constexpr double SectorReflectionUpdateInterval = 0.1;
constexpr double SectorReflectionTransitionSeconds = 0.1;
Camera3D SectorReflectionFaceCamera(Vector3 position, int face);

void MarkSectorReflectionProbeDirty(SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                    bool discontinuity);
bool CanStartSectorReflectionProbe(const SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                   bool preparing);
void PublishSectorReflectionProbe(SectorPbrEnvironment::LocalProbe &probe, double seconds,
                                  std::uint64_t capturedRevision);
bool SectorReflectionLightsMatch(const SectorPreviewDynamicPointLightUniform &a,
                                 const SectorPreviewDynamicPointLightUniform &b);
bool SectorReflectionLightAffectsProbe(const SectorPreviewDynamicPointLightUniform &light,
                                       const SectorCompiledReflectionProbe &probe);
bool SectorReflectionLightDiscontinuity(const SectorPreviewDynamicPointLightUniform *old,
                                        const SectorPreviewDynamicPointLightUniform *current);

} // namespace game
