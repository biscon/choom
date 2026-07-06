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
SectorEditorMaterialActionResult PasteMaterialToFields(
        TopologySurfaceEditTarget target,
        const TopologyMaterialPayload& payload,
        std::string& textureId,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult ApplySurfaceUvValue(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind,
        int component,
        float value);
SectorEditorMaterialActionResult ApplySurfaceUvValueToSettings(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind,
        int component,
        float value,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult ApplySurfaceDecalOpacity(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        float opacity);
SectorEditorMaterialActionResult ApplySurfaceDecalOpacityToLayer(
        TopologySurfaceEditTarget target,
        float opacity,
        SectorTopologyDecalLayer& decal);
SectorEditorMaterialActionResult ApplySurfaceDecalEmissive(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        bool emissive);
SectorEditorMaterialActionResult ApplySurfaceDecalEmissiveToLayer(
        TopologySurfaceEditTarget target,
        bool emissive,
        SectorTopologyDecalLayer& decal);
SectorEditorMaterialActionResult ApplySurfaceDecalTint(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        Vector3 tint);
SectorEditorMaterialActionResult ApplySurfaceDecalTintToLayer(
        TopologySurfaceEditTarget target,
        Vector3 tint,
        SectorTopologyDecalLayer& decal);
SectorEditorMaterialActionResult ApplySurfaceDecalBloomIntensity(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        float bloomIntensity);
SectorEditorMaterialActionResult ApplySurfaceDecalBloomIntensityToLayer(
        TopologySurfaceEditTarget target,
        float bloomIntensity,
        SectorTopologyDecalLayer& decal);
bool BuildDecalTintModal(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        DecalTintModalState& outModal,
        std::string& status);
SectorEditorMaterialActionResult ClearSurfaceDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorEditorMaterialActionResult ClearSurfaceDecalLayer(
        TopologySurfaceEditTarget target,
        SectorTopologyDecalLayer& decal);
SectorEditorMaterialActionResult ClearMiddleTexture(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorEditorMaterialActionResult ClearMiddleTextureSettings(
        TopologySurfaceEditTarget target,
        SectorTopologyWallPartSettings& middle);
SectorEditorMaterialActionResult ResetSurfaceUv(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind);
SectorEditorMaterialActionResult ResetSurfaceUvSettings(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorSurfaceKind surfaceKind,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult FitSelectedDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorEditorMaterialActionResult FitSelectedDecalToAuthoring(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult FitSelectedFlatDecal(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target);
SectorEditorMaterialActionResult FitSelectedFlatDecalToUv(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult FitSelectedWallMaterial(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        TopologyMaterialLayer layer);
SectorEditorMaterialActionResult FitSelectedWallMaterialToUv(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        TopologyMaterialLayer layer,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult AlignSelectedWallMaterialVertical(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer);
SectorEditorMaterialActionResult AlignSelectedWallMaterialVerticalToUv(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        SectorTopologyUvSettings& uv);
SectorEditorMaterialActionResult AlignSelectedWallMaterialU(
        SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        TopologyMaterialLayer layer);
SectorEditorMaterialActionResult AlignSelectedWallMaterialUToUv(
        const SectorTopologyMap& map,
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        TopologyMaterialLayer layer,
        SectorTopologyUvSettings& uv);

} // namespace game
