#pragma once

#include "sector_demo/SectorTopologyCreation.h"

#include <raylib.h>

#include <string>

namespace game {

struct PendingAuthoringLineDraw {
    bool active = false;
    SectorTopologyCoordPoint startPoint = {};
    int startVertexId = -1;
    std::string errorMessage;
};

struct PendingAuthoringRectangleDraw {
    bool active = false;
    SectorTopologyCoordPoint firstCorner = {};
    SectorTopologyCoordPoint currentCorner = {};
    std::string errorMessage;
};

struct PendingAuthoringInsertVertex {
    bool active = false;
    int lineId = -1;
    SectorTopologyCoordPoint previewPoint = {};
    bool hasPreviewPoint = false;
    std::string errorMessage;
};

enum class TopologySelectionKind {
    None,
    Sector,
    Vertex,
    SideDef,
    LineDef,
    StaticLight,
    StaticSpotLight,
    StaticRectLight,
    DynamicLight,
    DynamicSpotLight,
    DynamicRectLight
};

enum class SectorAuthoringSelectionKind {
    None,
    Line,
    Vertex,
    FaceAnchor,
    FogVolume,
    ReflectionProbe,
    LevelMarker,
    SoundEmitter,
    Trigger
};

struct SectorAuthoringSelectionTarget {
    SectorAuthoringSelectionKind kind = SectorAuthoringSelectionKind::None;
    int lineId = -1;
    int vertexId = -1;
    int faceAnchorId = -1;
    int fogVolumeId = -1;
    int reflectionProbeId = -1;
    int levelMarkerId = -1;
    int soundEmitterId = -1;
    int triggerId = -1;
};

enum class SectorSurfaceKind {
    None,
    Floor,
    Ceiling,
    Wall,
    LowerWall,
    UpperWall,
    Middle
};

struct SectorSurfaceRef {
    SectorSurfaceKind kind = SectorSurfaceKind::None;
    int topologySectorId = -1;
    int topologyLineDefId = -1;
    int topologySideDefId = -1;
    SectorTopologySideKind topologySide = SectorTopologySideKind::Front;
};

struct SectorSurfaceHit {
    bool hit = false;
    SectorSurfaceRef surface;
    Vector3 worldPosition = {};
    float distance = 0.0f;
};

struct AuthoringVertexDragState {
    bool active = false;
    int vertexId = -1;
    SectorTopologyCoordPoint originalPoint = {};
    SectorTopologyCoordPoint previewPoint = {};
    bool hasPreviewPoint = false;
    std::string errorMessage;
};

struct AuthoringFogVolumeDragState {
    bool active = false;
    int fogVolumeId = -1;
    SectorTopologyCoordPoint originalPoint = {};
    SectorTopologyCoordPoint previewPoint = {};
    bool hasPreviewPoint = false;
    bool previewResolved = false;
    std::string errorMessage;
};

struct AuthoringReflectionProbeDragState {
    bool active = false;
    int reflectionProbeId = -1;
    SectorTopologyCoordPoint originalPoint = {};
    SectorTopologyCoordPoint previewPoint = {};
    bool hasPreviewPoint = false;
    bool previewResolved = false;
    std::string errorMessage;
};

enum class SpotLightHandle {
    Origin,
    Target
};

enum class SectorEditorPickKind {
    None,
    RuntimeObject,
    DynamicSpotLight,
    DynamicRectLight,
    DynamicLight,
    StaticSpotLight,
    StaticRectLight,
    StaticLight,
    AuthoringVertex,
    AuthoringLine,
    AuthoringFaceAnchor,
    AuthoringFogVolume,
    AuthoringReflectionProbe,
    LevelMarker,
    SoundEmitter,
    Trigger
};

struct SectorEditorPickTarget {
    SectorEditorPickKind kind = SectorEditorPickKind::None;
    int id = -1;
    SpotLightHandle spotHandle = SpotLightHandle::Origin;
};

struct SectorEditorPickCandidate {
    SectorEditorPickTarget target;
    float distance2 = 0.0f;
};

struct SelectDragArmState {
    bool active = false;
    SectorEditorPickTarget target;
    Vector2 pressPosition = {};
};

struct RuntimeObjectDragState {
    bool active = false;
    int objectId = -1;
    Vector3 originalPosition = {};
    Vector3 snappedPosition = {};
};

struct LevelMarkerDragState {
    bool active = false;
    int markerId = -1;
    SectorCoord originalX = 0;
    SectorCoord originalZ = 0;
    SectorCoord previewX = 0;
    SectorCoord previewZ = 0;
};

struct SoundEmitterDragState {
    bool active = false;
    int emitterId = -1;
    SectorCoord originalX = 0;
    SectorCoord originalZ = 0;
    SectorCoord previewX = 0;
    SectorCoord previewZ = 0;
};

} // namespace game
