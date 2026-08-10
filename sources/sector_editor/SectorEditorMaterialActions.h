#pragma once

#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
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
const SectorTopologyUvSettings* UvForMaterialSurface(
        const SectorTopologyMap& map,
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
SectorEditorMaterialActionResult PasteMaterialToFields(
        TopologySurfaceEditTarget target,
        const TopologyMaterialPayload& payload,
        std::string& textureId,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult ApplySurfaceUvValueToSettings(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind,
        int component,
        float value,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult ApplySurfaceDecalOpacityToLayer(
        TopologySurfaceEditTarget target,
        float opacity,
        SectorTopologyDecalLayer& decal);
SectorEditorMaterialActionResult ApplySurfaceDecalEmissiveToLayer(
        TopologySurfaceEditTarget target,
        bool emissive,
        SectorTopologyDecalLayer& decal);
SectorEditorMaterialActionResult ApplySurfaceDecalTintToLayer(
        TopologySurfaceEditTarget target,
        Vector3 tint,
        SectorTopologyDecalLayer& decal);
SectorEditorMaterialActionResult ApplySurfaceDecalEmissiveStrengthToLayer(
        TopologySurfaceEditTarget target,
        float emissiveStrength,
        SectorTopologyDecalLayer& decal);
bool BuildDecalTintModal(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        DecalTintModalState& outModal,
        std::string& status);
SectorEditorMaterialActionResult ClearSurfaceDecalLayer(
        TopologySurfaceEditTarget target,
        SectorTopologyDecalLayer& decal);
SectorEditorMaterialActionResult ClearMiddleTextureSettings(
        TopologySurfaceEditTarget target,
        SectorTopologyWallPartSettings& middle);
SectorEditorMaterialActionResult ResetSurfaceUvSettings(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult FitSelectedDecalToAuthoring(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult FitSelectedFlatDecalToUv(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult FitSelectedWallMaterialToUv(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        TopologyMaterialLayer layer,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult AlignSelectedWallMaterialVerticalToUv(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult AlignSelectedWallMaterialUToUv(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        TopologyMaterialLayer layer,
        SectorTopologyUvSettings& uv);

} // namespace game
