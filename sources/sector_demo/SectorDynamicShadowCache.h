#pragma once

#include "sector_demo/SectorDynamicPointLightSelection.h"
#include <cstring>

namespace game {

// The stored matrix/light describe the actual atlas contents, not a pending pose.
struct SectorDynamicShadowTileState {
    SectorPreviewDynamicSpotLightShadowMatrix matrix{};
    SectorPreviewDynamicPointLightUniform light{};
    bool assigned = false;
    bool valid = false;
    bool dirty = true;
    uint64_t dirtySerial = 0;
};

inline bool SectorShadowMatrixMatches(const SectorPreviewDynamicSpotLightShadowMatrix& a,
        const SectorPreviewDynamicSpotLightShadowMatrix& b)
{
    return a.lightId == b.lightId && a.shadowSlot == b.shadowSlot && a.kind == b.kind
            && a.cubeFace == b.cubeFace && a.lightRadius == b.lightRadius
            && std::memcmp(&a.lightPosition,&b.lightPosition,sizeof(Vector3)) == 0
            && std::memcmp(&a.lightViewProjection,&b.lightViewProjection,sizeof(Matrix)) == 0;
}

inline bool CanRetainSectorSwayShadow(const SectorDynamicShadowTileState& tile,
        const SectorPreviewDynamicSpotLightShadowMatrix& requested,
        const SectorPreviewDynamicPointLightUniform& light)
{
    const auto& previous = tile.light;
    return tile.valid && light.positionSway.enabled && light.castsShadow
            && light.kind == SectorPreviewDynamicLightKind::Point
            && previous.kind == light.kind && previous.lightId == light.lightId
            && previous.positionSway == light.positionSway
            && previous.radius == light.radius && previous.castsShadow == light.castsShadow
            && previous.shadowBias == light.shadowBias
            && previous.shadowStrength == light.shadowStrength
            && previous.shadowSoftness == light.shadowSoftness
            && std::memcmp(&previous.basePosition,&light.basePosition,sizeof(Vector3)) == 0
            && tile.matrix.lightId == requested.lightId && tile.matrix.kind == requested.kind
            && tile.matrix.shadowSlot == requested.shadowSlot && tile.matrix.cubeFace == requested.cubeFace;
}

inline void RefreshSectorDynamicShadowTiles(
        const std::vector<SectorPreviewDynamicPointLightUniform>& lights,
        const std::vector<SectorPreviewDynamicSpotLightShadowMatrix>& matrices,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& casters,
        std::array<SectorDynamicShadowTileState, MaxDynamicSpotLightShadowCasters>& tiles,
        uint64_t& nextSerial)
{
    for (auto& tile : tiles) tile.assigned = false;
    for (const auto& matrix : matrices) {
        if (matrix.shadowSlot < 0 || static_cast<size_t>(matrix.shadowSlot) >= tiles.size()
                || matrix.dynamicLightIndex < 0 || static_cast<size_t>(matrix.dynamicLightIndex) >= lights.size()) continue;
        auto& tile = tiles[matrix.shadowSlot];
        const auto& light = lights[matrix.dynamicLightIndex];
        const bool matches = tile.valid && SectorShadowMatrixMatches(tile.matrix,matrix)
                && (!light.positionSway.enabled || CanRetainSectorSwayShadow(tile,matrix,light));
        const bool retain = CanRetainSectorSwayShadow(tile,matrix,light);
        tile.assigned = true;
        if (!matches) {
            if (!retain) tile.valid = false;
            tile.dirty = true;
            if (tile.dirtySerial == 0) tile.dirtySerial = nextSerial++;
        }
    }
    // Cube faces are one cache entry: invalidation and queue age are shared.
    for (const auto& caster : casters) {
        if (caster.shadowSlot < 0 || caster.shadowSlotCount <= 0
                || caster.dynamicLightIndex < 0 || static_cast<size_t>(caster.dynamicLightIndex) >= lights.size()
                || static_cast<size_t>(caster.shadowSlot+caster.shadowSlotCount) > tiles.size()) continue;
        uint64_t serial = 0;
        bool dirty = false, valid = true;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const auto& tile = tiles[caster.shadowSlot+offset];
            dirty |= tile.dirty; valid &= tile.assigned && tile.valid;
            if (tile.dirtySerial && (!serial || tile.dirtySerial < serial)) serial = tile.dirtySerial;
        }
        if (!dirty && valid) continue;
        if (!serial) serial = nextSerial++;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            auto& tile = tiles[caster.shadowSlot+offset];
            // Finite face budgets are used by reflection captures. They must be
            // allowed to finish remaining faces across frames. Sway captures are disabled.
            if (!valid && lights[caster.dynamicLightIndex].positionSway.enabled) tile.valid = false;
            tile.dirty = true; tile.dirtySerial = serial;
        }
    }
}

inline bool SectorShadowSpanValid(const SectorPreviewDynamicSpotLightShadowCaster& caster,
        const std::array<SectorDynamicShadowTileState, MaxDynamicSpotLightShadowCasters>& tiles)
{
    if (caster.shadowSlot < 0 || caster.shadowSlotCount <= 0
            || static_cast<size_t>(caster.shadowSlot+caster.shadowSlotCount) > tiles.size()) return false;
    for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
        const auto& tile = tiles[caster.shadowSlot+offset];
        if (!tile.assigned || !tile.valid) return false;
    }
    return true;
}

inline void PublishSectorSwayShadowPositions(
        std::vector<SectorPreviewDynamicPointLightUniform>& lights,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& casters,
        const std::array<SectorDynamicShadowTileState, MaxDynamicSpotLightShadowCasters>& tiles)
{
    for (const auto& caster : casters) {
        if (caster.dynamicLightIndex < 0 || static_cast<size_t>(caster.dynamicLightIndex) >= lights.size()) continue;
        auto& light = lights[caster.dynamicLightIndex];
        if (light.positionSway.enabled && SectorShadowSpanValid(caster,tiles))
            light.position = tiles[caster.shadowSlot].matrix.lightPosition;
    }
}
} // namespace game
