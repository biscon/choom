#pragma once

#include "sector_editor/SectorEditorTypes.h"

#include <variant>

namespace game {

enum class SectorEditorConfigKind {
    None,
    Sector,
    Door,
    StaticPointLight,
    StaticSpotLight,
    StaticRectLight,
    DynamicPointLight,
    DynamicSpotLight,
    DynamicRectLight,
    SurfaceFloor,
    SurfaceCeiling,
    SurfaceWall,
    SurfaceLower,
    SurfaceUpper
};

struct SectorEditorConfigTarget {
    SectorEditorConfigKind kind = SectorEditorConfigKind::None;
    int id = -1;
    TopologySurfaceEditTarget surface;
};

using SectorEditorConfigPayload = std::variant<
        std::monostate,
        SectorAuthoringFaceAnchor,
        SectorPlacedDoor,
        SectorTopologyStaticPointLight,
        SectorTopologyStaticSpotLight,
        SectorTopologyStaticRectLight,
        SectorTopologyDynamicPointLight,
        SectorTopologyDynamicSpotLight,
        SectorTopologyDynamicRectLight,
        TopologyMaterialPayload>;

struct SectorEditorConfigClipboardState {
    SectorEditorConfigKind kind = SectorEditorConfigKind::None;
    SectorEditorConfigPayload payload;
};

inline const char* SectorEditorConfigKindName(SectorEditorConfigKind kind)
{
    switch (kind) {
        case SectorEditorConfigKind::Sector: return "sector";
        case SectorEditorConfigKind::Door: return "door";
        case SectorEditorConfigKind::StaticPointLight: return "static point light";
        case SectorEditorConfigKind::StaticSpotLight: return "static spot light";
        case SectorEditorConfigKind::StaticRectLight: return "static rect light";
        case SectorEditorConfigKind::DynamicPointLight: return "dynamic point light";
        case SectorEditorConfigKind::DynamicSpotLight: return "dynamic spot light";
        case SectorEditorConfigKind::DynamicRectLight: return "dynamic rect light";
        case SectorEditorConfigKind::SurfaceFloor: return "floor material";
        case SectorEditorConfigKind::SurfaceCeiling: return "ceiling material";
        case SectorEditorConfigKind::SurfaceWall: return "wall material";
        case SectorEditorConfigKind::SurfaceLower: return "lower-wall material";
        case SectorEditorConfigKind::SurfaceUpper: return "upper-wall material";
        case SectorEditorConfigKind::None: return "config";
    }
    return "config";
}

inline SectorEditorConfigKind SectorEditorSurfaceConfigKind(
        TopologySurfaceEditTargetKind kind)
{
    switch (kind) {
        case TopologySurfaceEditTargetKind::SectorFloor:
            return SectorEditorConfigKind::SurfaceFloor;
        case TopologySurfaceEditTargetKind::SectorCeiling:
            return SectorEditorConfigKind::SurfaceCeiling;
        case TopologySurfaceEditTargetKind::SideDefWall:
            return SectorEditorConfigKind::SurfaceWall;
        case TopologySurfaceEditTargetKind::SideDefLower:
            return SectorEditorConfigKind::SurfaceLower;
        case TopologySurfaceEditTargetKind::SideDefUpper:
            return SectorEditorConfigKind::SurfaceUpper;
        case TopologySurfaceEditTargetKind::SideDefMiddle:
        case TopologySurfaceEditTargetKind::None:
            return SectorEditorConfigKind::None;
    }
    return SectorEditorConfigKind::None;
}

} // namespace game
