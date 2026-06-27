#pragma once

#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <string>

namespace game {

struct SectorEditorMaterialActionResult {
    bool changed = false;
    bool resetSurface3DUi = false;
    bool resetSectorUvInputs = false;
    bool resetSideDefUvInputs = false;
    bool resetDecalInputs = false;
    bool closeDecalTintModal = false;
    std::string status;
};

bool IsValidMaterialSurfaceTarget(const SectorTopologyMap& map, TopologySurfaceEditTarget target);
const SectorTopologyDecalLayer* DecalForMaterialSurface(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorTopologyDecalLayer* MutableDecalForMaterialSurface(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
const SectorTopologyUvSettings* UvForMaterialSurface(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer);
SectorTopologyUvSettings* MutableUvForMaterialSurface(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer);
bool IsMaterialDecalAssigned(const SectorTopologyMap& map, TopologySurfaceEditTarget target);
std::string CurrentTextureForMaterialSurface(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer);

bool CopyMaterialSurface(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialPayload& outPayload,
        std::string& status);
SectorEditorMaterialActionResult PasteMaterialSurface(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        const TopologyMaterialPayload& payload);
SectorEditorMaterialActionResult ApplySurfaceUvValue(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind,
        int component,
        float value);
SectorEditorMaterialActionResult ApplySurfaceDecalOpacity(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        float opacity);
SectorEditorMaterialActionResult ApplySurfaceDecalEmissive(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        bool emissive);
SectorEditorMaterialActionResult ApplySurfaceDecalTint(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        Vector3 tint);
SectorEditorMaterialActionResult ApplySurfaceDecalBloomIntensity(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        float bloomIntensity);
bool BuildDecalTintModal(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        DecalTintModalState& outModal,
        std::string& status);
SectorEditorMaterialActionResult ClearSurfaceDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorEditorMaterialActionResult ClearMiddleTexture(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorEditorMaterialActionResult ResetSurfaceUv(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind);
SectorEditorMaterialActionResult FitSelectedDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorEditorMaterialActionResult FitSelectedFlatDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorEditorMaterialActionResult FitSelectedWallMaterial(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        TopologyMaterialLayer layer);
SectorEditorMaterialActionResult AlignSelectedWallMaterialVertical(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer);
SectorEditorMaterialActionResult AlignSelectedWallMaterialU(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        TopologyMaterialLayer layer);

} // namespace game
