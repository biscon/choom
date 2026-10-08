#pragma once
#include "sector_demo/particles/SectorParticleSettings.h"
#include <algorithm>

namespace game {
enum SectorParticleInspectorRow {
    ParticleInspectorTitle, ParticleInspectorPreview, ParticleInspectorPlayback, ParticleInspectorViewControls, ParticleInspectorPreviewStats, ParticleInspectorName, ParticleInspectorEnabled, ParticleInspectorPreset, ParticleInspectorReapply,
    ParticleInspectorPresetName, ParticleInspectorSavePreset, ParticleInspectorPositionX, ParticleInspectorHeight, ParticleInspectorPositionZ, ParticleInspectorYaw, ParticleInspectorPitch, ParticleInspectorScale, ParticleInspectorIntensity, ParticleInspectorTimeScale, ParticleInspectorSpeed,
    ParticleInspectorLifetime, ParticleInspectorSpread, ParticleInspectorTintR, ParticleInspectorTintG, ParticleInspectorTintB, ParticleInspectorTintA, ParticleInspectorSmoke, ParticleInspectorEmbers, ParticleInspectorSwirl, ParticleInspectorBurstInterval,
    ParticleInspectorBurstCount, ParticleInspectorAdvanced, ParticleInspectorShape, ParticleInspectorWidth, ParticleInspectorDepth, ParticleInspectorBoxHeight, ParticleInspectorDriftX, ParticleInspectorDriftY, ParticleInspectorDriftZ,
    ParticleInspectorTurbulence, ParticleInspectorGravity, ParticleInspectorDrag, ParticleInspectorEmission, ParticleInspectorCollision, ParticleInspectorRestitution, ParticleInspectorDistance, ParticleInspectorTextures,
    ParticleInspectorTextureRole, ParticleInspectorTexturePath, ParticleInspectorColumns, ParticleInspectorRows, ParticleInspectorFrames, ParticleInspectorFps, ParticleInspectorRandomStart, ParticleInspectorMessage, ParticleInspectorDuplicate, ParticleInspectorDelete
};
// Drawing and measuring use exactly the same conditional row sequence.
template<class Visit> void WalkSectorParticleInspectorRows(const SectorAuthoringParticleEmitter& e,
        bool advanced, bool textures, bool message, Visit visit)
{
    for (SectorParticleInspectorRow r : {ParticleInspectorTitle, ParticleInspectorPreview, ParticleInspectorPlayback, ParticleInspectorViewControls, ParticleInspectorPreviewStats, ParticleInspectorName, ParticleInspectorEnabled, ParticleInspectorPreset,
            ParticleInspectorReapply, ParticleInspectorPresetName, ParticleInspectorSavePreset, ParticleInspectorPositionX, ParticleInspectorHeight, ParticleInspectorPositionZ, ParticleInspectorYaw, ParticleInspectorPitch, ParticleInspectorScale,
            ParticleInspectorIntensity, ParticleInspectorTimeScale, ParticleInspectorSpeed, ParticleInspectorLifetime, ParticleInspectorSpread, ParticleInspectorTintR, ParticleInspectorTintG, ParticleInspectorTintB, ParticleInspectorTintA}) visit(r);
    if (e.settings.preset == SectorParticlePreset::Fire) { visit(ParticleInspectorSmoke); visit(ParticleInspectorEmbers); }
    if (e.settings.preset == SectorParticlePreset::Vapor || e.settings.preset == SectorParticlePreset::MistSwirl) visit(ParticleInspectorSwirl);
    if (e.settings.emission != engine::ParticleEmission::Continuous) { visit(ParticleInspectorBurstInterval); visit(ParticleInspectorBurstCount); }
    visit(ParticleInspectorAdvanced);
    if (advanced) {
        visit(ParticleInspectorShape);
        if (e.settings.shape != engine::ParticleShape::Point) { visit(ParticleInspectorWidth); visit(ParticleInspectorDepth); }
        if (e.settings.shape == engine::ParticleShape::Box) visit(ParticleInspectorBoxHeight);
        for (SectorParticleInspectorRow r : {ParticleInspectorDriftX, ParticleInspectorDriftY, ParticleInspectorDriftZ, ParticleInspectorTurbulence, ParticleInspectorGravity, ParticleInspectorDrag, ParticleInspectorEmission, ParticleInspectorCollision}) visit(r);
        if (e.settings.collision == engine::ParticleCollision::Bounce) visit(ParticleInspectorRestitution);
        visit(ParticleInspectorDistance); visit(ParticleInspectorTextures);
        if (textures)
            for (SectorParticleInspectorRow r : {ParticleInspectorTextureRole, ParticleInspectorTexturePath, ParticleInspectorColumns, ParticleInspectorRows, ParticleInspectorFrames, ParticleInspectorFps, ParticleInspectorRandomStart}) visit(r);
    }
    if (message) visit(ParticleInspectorMessage);
    visit(ParticleInspectorDuplicate); visit(ParticleInspectorDelete);
}
inline bool IsSectorParticleInspectorLabelled(SectorParticleInspectorRow row)
{
    return row == ParticleInspectorName || row == ParticleInspectorPreset || row == ParticleInspectorPresetName || (row >= ParticleInspectorPositionX && row <= ParticleInspectorBurstCount)
            || (row >= ParticleInspectorShape && row <= ParticleInspectorDistance) || (row >= ParticleInspectorTextureRole && row <= ParticleInspectorFps);
}
inline float SectorParticleInspectorLabelHeight(float height,
        SectorParticleInspectorRow row = ParticleInspectorName)
{
    // The launch-speed label wraps in the fixed-width inspector; reserve two lines.
    return std::max(22.0f, height * (row == ParticleInspectorSpeed ? 2.0f : 1.5f));
}
inline float SectorParticleInspectorRowExtent(SectorParticleInspectorRow row, float height, float gap)
{
    if (row == ParticleInspectorPreview) return 180 + gap;
    if (row == ParticleInspectorMessage) return 72 + gap;
    if (row == ParticleInspectorViewControls) return height * 2 + gap * 2;
    if (row == ParticleInspectorPlayback) return height * 3 + gap * 3;
    return height + gap + (IsSectorParticleInspectorLabelled(row) ? SectorParticleInspectorLabelHeight(height, row) + gap : 0);
}
} // namespace game
