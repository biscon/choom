#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"

#include <raylib.h>

#include <vector>

namespace game {

enum class SectorEditorSelectionTargetKind {
    None,
    TopologySector,
    TopologyVertex,
    TopologyLineDef,
    TopologySideDef,
    AuthoringVertex,
    AuthoringLine,
    AuthoringFaceAnchor,
    AuthoringFogVolume,
    AuthoringReflectionProbe,
    AuthoringLevelMarker,
    AuthoringTrigger,
    RuntimeObject,
    StaticLight,
    StaticSpotLight,
    DynamicLight,
    DynamicSpotLight,
    PreviewSurface
};

struct SectorEditorSelectionTarget {
    SectorEditorSelectionTargetKind kind = SectorEditorSelectionTargetKind::None;
    int id = -1;
    int secondaryId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
    TopologyWallPart wallPart = TopologyWallPart::Wall;
    SpotLightHandle spotHandle = SpotLightHandle::Origin;
    SectorSurfaceRef surface;
};

inline bool IsValid(SectorEditorSelectionTarget target)
{
    if (target.kind == SectorEditorSelectionTargetKind::None) {
        return false;
    }
    if (target.kind == SectorEditorSelectionTargetKind::PreviewSurface) {
        return target.surface.kind != SectorSurfaceKind::None;
    }
    return target.id >= 0;
}

inline bool IsTopologyTarget(SectorEditorSelectionTarget target)
{
    switch (target.kind) {
        case SectorEditorSelectionTargetKind::TopologySector:
        case SectorEditorSelectionTargetKind::TopologyVertex:
        case SectorEditorSelectionTargetKind::TopologyLineDef:
        case SectorEditorSelectionTargetKind::TopologySideDef:
            return target.id >= 0;
        case SectorEditorSelectionTargetKind::None:
        case SectorEditorSelectionTargetKind::AuthoringVertex:
        case SectorEditorSelectionTargetKind::AuthoringLine:
        case SectorEditorSelectionTargetKind::AuthoringFaceAnchor:
        case SectorEditorSelectionTargetKind::AuthoringFogVolume:
        case SectorEditorSelectionTargetKind::AuthoringReflectionProbe:
        case SectorEditorSelectionTargetKind::AuthoringLevelMarker:
        case SectorEditorSelectionTargetKind::AuthoringTrigger:
        case SectorEditorSelectionTargetKind::RuntimeObject:
        case SectorEditorSelectionTargetKind::StaticLight:
        case SectorEditorSelectionTargetKind::StaticSpotLight:
        case SectorEditorSelectionTargetKind::DynamicLight:
        case SectorEditorSelectionTargetKind::DynamicSpotLight:
        case SectorEditorSelectionTargetKind::PreviewSurface:
            return false;
    }
    return false;
}

inline bool IsAuthoringTarget(SectorEditorSelectionTarget target)
{
    switch (target.kind) {
        case SectorEditorSelectionTargetKind::AuthoringVertex:
        case SectorEditorSelectionTargetKind::AuthoringLine:
        case SectorEditorSelectionTargetKind::AuthoringFaceAnchor:
        case SectorEditorSelectionTargetKind::AuthoringFogVolume:
        case SectorEditorSelectionTargetKind::AuthoringReflectionProbe:
        case SectorEditorSelectionTargetKind::AuthoringLevelMarker:
        case SectorEditorSelectionTargetKind::AuthoringTrigger:
            return target.id >= 0;
        case SectorEditorSelectionTargetKind::None:
        case SectorEditorSelectionTargetKind::TopologySector:
        case SectorEditorSelectionTargetKind::TopologyVertex:
        case SectorEditorSelectionTargetKind::TopologyLineDef:
        case SectorEditorSelectionTargetKind::TopologySideDef:
        case SectorEditorSelectionTargetKind::RuntimeObject:
        case SectorEditorSelectionTargetKind::StaticLight:
        case SectorEditorSelectionTargetKind::StaticSpotLight:
        case SectorEditorSelectionTargetKind::DynamicLight:
        case SectorEditorSelectionTargetKind::DynamicSpotLight:
        case SectorEditorSelectionTargetKind::PreviewSurface:
            return false;
    }
    return false;
}

inline bool IsRuntimeObjectTarget(SectorEditorSelectionTarget target)
{
    return target.kind == SectorEditorSelectionTargetKind::RuntimeObject && target.id >= 0;
}

inline bool IsPlacedObjectTarget(SectorEditorSelectionTarget target)
{
    return IsRuntimeObjectTarget(target);
}

inline bool IsLightTarget(SectorEditorSelectionTarget target)
{
    switch (target.kind) {
        case SectorEditorSelectionTargetKind::StaticLight:
        case SectorEditorSelectionTargetKind::StaticSpotLight:
        case SectorEditorSelectionTargetKind::DynamicLight:
        case SectorEditorSelectionTargetKind::DynamicSpotLight:
            return target.id >= 0;
        case SectorEditorSelectionTargetKind::None:
        case SectorEditorSelectionTargetKind::TopologySector:
        case SectorEditorSelectionTargetKind::TopologyVertex:
        case SectorEditorSelectionTargetKind::TopologyLineDef:
        case SectorEditorSelectionTargetKind::TopologySideDef:
        case SectorEditorSelectionTargetKind::AuthoringVertex:
        case SectorEditorSelectionTargetKind::AuthoringLine:
        case SectorEditorSelectionTargetKind::AuthoringFaceAnchor:
        case SectorEditorSelectionTargetKind::AuthoringFogVolume:
        case SectorEditorSelectionTargetKind::AuthoringReflectionProbe:
        case SectorEditorSelectionTargetKind::AuthoringLevelMarker:
        case SectorEditorSelectionTargetKind::AuthoringTrigger:
        case SectorEditorSelectionTargetKind::RuntimeObject:
        case SectorEditorSelectionTargetKind::PreviewSurface:
            return false;
    }
    return false;
}

inline bool IsPreviewSurfaceTarget(SectorEditorSelectionTarget target)
{
    return target.kind == SectorEditorSelectionTargetKind::PreviewSurface
            && target.surface.kind != SectorSurfaceKind::None;
}

struct SectorEditorPickContext;
struct SectorEditorSelectionContext;
struct SectorEditorMoveContext;

using SectorEditorPickCandidateList = std::vector<SectorEditorPickCandidate>;

struct SectorEditorPickProvider {
    void (*collectPickCandidates)(SectorEditorPickContext&, SectorEditorPickCandidateList&) = nullptr;
};

struct SectorEditorSelectionProvider {
    bool (*select)(SectorEditorSelectionContext&, SectorEditorSelectionTarget) = nullptr;
    bool (*isStillValid)(SectorEditorSelectionContext&, SectorEditorSelectionTarget) = nullptr;
};

struct SectorEditorMoveProvider {
    bool (*canMove)(SectorEditorMoveContext&, SectorEditorSelectionTarget) = nullptr;
    bool (*beginMove)(SectorEditorMoveContext&, SectorEditorSelectionTarget, Vector2) = nullptr;
    void (*updateMove)(SectorEditorMoveContext&, Vector2) = nullptr;
    void (*finishMove)(SectorEditorMoveContext&) = nullptr;
    void (*cancelMove)(SectorEditorMoveContext&, const char*) = nullptr;
};

} // namespace game
