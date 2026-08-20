#pragma once

#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"

#include <raylib.h>

#include <cstdint>

namespace game {

struct SectorAtmosphereScissorRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool Empty() const { return width <= 0 || height <= 0; }
};

Vector3 ComputeSectorAtmosphereYawedHalfExtents(
        Vector3 localHalfExtents,
        float yawRadians);

SectorAtmosphereScissorRect ProjectSectorAtmosphereBoundsToScissor(
        const Camera3D& camera,
        float aspectRatio,
        float nearPlane,
        Vector3 boundsMin,
        Vector3 boundsMax,
        int targetWidth,
        int targetHeight);

SectorAtmosphereScissorRect UnionSectorAtmosphereScissors(
        SectorAtmosphereScissorRect left,
        SectorAtmosphereScissorRect right,
        int targetWidth,
        int targetHeight);

float SectorAtmosphereScissorCoverage(
        SectorAtmosphereScissorRect rect,
        int targetWidth,
        int targetHeight);

bool SectorAtmosphereDynamicLightIntersectsBounds(
        const SectorBillboardDynamicLightContext& lights,
        int lightIndex,
        Vector3 boundsMin,
        Vector3 boundsMax);

std::uint32_t BuildSectorAtmosphereDynamicLightMask(
        const SectorBillboardDynamicLightContext& lights,
        Vector3 boundsMin,
        Vector3 boundsMax);

} // namespace game
