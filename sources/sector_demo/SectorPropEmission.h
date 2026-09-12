#pragma once
#include "engine/assets/ModelAssets.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace game {
// Indexed runtime storage is sized while loading, never looked up by name in draw.
using SectorPropEmissionColors = std::vector<std::optional<Vector3>>;
struct SectorSavedEmissionColor { std::string material; Vector3 color{}; };

inline bool SetSectorPropEmissionColor(SectorPropEmissionColors& colors,
        const engine::ModelAsset& asset, const std::string& name,
        const std::optional<Vector3>& color, std::string& error)
{
    if (name.empty()) { error = "material name is empty"; return false; }
    if (color) for (float c : {color->x, color->y, color->z})
        if (!std::isfinite(c) || c < 0 || c > 1) { error = "colour must be finite linear RGB in 0..1"; return false; }
    const auto found = std::find(asset.materialNames.begin(), asset.materialNames.end(), name);
    if (found == asset.materialNames.end()) { error = "material was not found"; return false; }
    if (std::find(found+1, asset.materialNames.end(), name) != asset.materialNames.end()) {
        error = "material name is ambiguous"; return false;
    }
    const size_t index = static_cast<size_t>(found-asset.materialNames.begin());
    if (index >= colors.size()) { error = "prop material state is not ready"; return false; }
    colors[index] = color;
    error.clear();
    return true;
}

inline Vector3 SectorPropEmissionFactor(const SectorPropEmissionColors& colors, int index, Vector3 authored)
{
    return index >= 0 && static_cast<size_t>(index) < colors.size() && colors[index] ? *colors[index] : authored;
}

inline std::vector<SectorSavedEmissionColor> CaptureSectorPropEmissionColors(
        const SectorPropEmissionColors& colors, const engine::ModelAsset* asset)
{
    std::vector<SectorSavedEmissionColor> result;
    if (asset) for (size_t i = 0; i < colors.size() && i < asset->materialNames.size(); ++i)
        if (colors[i]) result.push_back({asset->materialNames[i], *colors[i]});
    return result;
}

inline void RestoreSectorPropEmissionColors(SectorPropEmissionColors& colors,
        const engine::ModelAsset* asset, const std::vector<SectorSavedEmissionColor>& saved)
{
    if (!asset) return;
    colors.assign(asset->materials.size(), std::nullopt);
    for (const auto& value : saved) {
        std::string error;
        if (!SetSectorPropEmissionColor(colors, *asset, value.material, value.color, error))
            TraceLog(LOG_WARNING, "[Prop emission] %s: %s", value.material.c_str(), error.c_str());
    }
}
} // namespace game
