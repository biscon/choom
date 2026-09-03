#pragma once

#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTopologyCreation.h"

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct CachedTopologyOutlineSegment {
    Vector2 a = {};
    Vector2 b = {};
    bool hole = false;
};

struct CachedTopologySectorDraw {
    int sectorId = -1;
    std::string label;
    Vector2 labelCenter = {};
    std::vector<Vector2> fillTrianglePoints;
    std::vector<CachedTopologyOutlineSegment> outlineSegments;
};

struct CachedTopologyLineDraw {
    int lineDefId = -1;
    int frontSideDefId = -1;
    int backSideDefId = -1;
    Vector2 start = {};
    Vector2 end = {};
    bool validEndpoints = false;
    bool hasFront = false;
    bool hasBack = false;
    bool hasPartialEndpoint = false;
    Vector2 partialEndpoint = {};
};

struct CachedTopologyVertexDraw {
    int vertexId = -1;
    SectorTopologyCoordPoint point = {};
    Vector2 map = {};
};

struct CachedTopologyLightDraw {
    int lightId = -1;
    Vector2 map = {};
    Color color = WHITE;
    float radiusPixelsAtZoomOne = 0.0f;
    float sourceRadiusPixelsAtZoomOne = 0.0f;
};

struct CachedTopologySpotLightDraw {
    int lightId = -1;
    Vector2 origin = {};
    Vector2 target = {};
    Color color = WHITE;
    float range = 0.0f;
    float innerConeDegrees = 0.0f;
    float outerConeDegrees = 0.0f;
};

struct CachedRuntimeObjectDraw {
    int objectId = -1;
    std::string definitionId;
    Vector2 map = {};
    float yawRadians = 0.0f;
    bool definitionKnown = false;
    bool isDoor = false;
    bool isWindow = false;
    bool isDuctAccess = false;
    bool isNpc = false;
    bool isItem = false;
    std::string itemLabel;
    bool doorFootprintValid = false;
    Color windowTint = WHITE;
    Vector2 doorCorners[4] = {};
    Vector2 doorEndpointA = {};
    Vector2 doorEndpointB = {};
    bool doorModelMetadataValid = true;
    bool doorSwingGuideValid = false;
    bool doorSwingIntoBack = false;
    Vector2 doorHinge = {};
    Vector2 doorOpenCorners[4] = {};
    Vector2 doorOpenFreeEdge = {};
    float doorSwingArcRadius = 0.0f;
    float doorSwingArcStartRadians = 0.0f;
    float doorSwingArcSweepRadians = 0.0f;
};

struct CachedAuthoringLevelMarkerDraw {
    int markerId = -1;
    std::string referenceId;
    Vector2 map = {};
    float orientationDegrees = 0.0f;
};

struct CachedAuthoringSoundEmitterDraw {
    int emitterId = -1;
    std::string referenceId;
    Vector2 map = {};
};

struct CachedAuthoringTriggerDraw {
    int triggerId = -1;
    std::string id;
    bool enabled = true;
    std::vector<Vector2> points;
    std::vector<Vector2> fillTrianglePoints;
};

struct CachedStructuralPrimitiveDraw {
    int primitiveId = -1;
    SectorStructuralPrimitiveKind kind = SectorStructuralPrimitiveKind::Box;
    bool enabled = true;
    bool circular = false;
    Vector2 center = {};
    float radius = 0.0f;
    Vector2 ascentDirection = {0.0f, 1.0f};
    std::vector<Vector2> points;
};

struct CachedAuthoringVertexDraw {
    int vertexId = -1;
    SectorTopologyCoordPoint point = {};
    Vector2 map = {};
};

struct CachedAuthoringLineDraw {
    int lineId = -1;
    Vector2 start = {};
    Vector2 end = {};
    bool validEndpoints = false;
    bool hasPartialEndpoint = false;
    Vector2 partialEndpoint = {};
};

struct CachedAuthoringFaceHighlightDraw {
    int faceAnchorId = -1;
    int topologySectorId = -1;
    bool isVoid = false;
    std::vector<CachedTopologyOutlineSegment> outlineSegments;
};

struct CachedAuthoringDiagnosticDraw {
    SectorAuthoringDerivationDiagnosticKind kind =
            SectorAuthoringDerivationDiagnosticKind::AuthoringReference;
    SectorAuthoringValidationSeverity severity = SectorAuthoringValidationSeverity::Error;
    int objectId = -1;
    int relatedObjectId = -1;
    Vector2 map = {};
    bool hasPosition = false;
    std::string message;
};

struct SectorEditorTopologyRenderCache {
    bool valid = false;
    uint64_t revision = 0;
    uint64_t swingDoorCatalogRevision = 0;
    uint64_t itemRegistryRevision = 0;
    std::string warning;
    std::vector<CachedTopologySectorDraw> sectors;
    std::vector<CachedTopologyLineDraw> lineDefs;
    std::vector<CachedTopologyVertexDraw> vertices;
    std::vector<CachedTopologyLightDraw> staticLights;
    std::vector<CachedTopologySpotLightDraw> staticSpotLights;
    std::vector<CachedTopologyLightDraw> dynamicLights;
    std::vector<CachedTopologySpotLightDraw> dynamicSpotLights;
    std::vector<CachedRuntimeObjectDraw> runtimeObjects;
    std::vector<CachedAuthoringLevelMarkerDraw> levelMarkers;
    std::vector<CachedAuthoringSoundEmitterDraw> soundEmitters;
    std::vector<CachedAuthoringTriggerDraw> triggers;
    std::vector<CachedStructuralPrimitiveDraw> structuralPrimitives;
    std::vector<CachedAuthoringLineDraw> authoringLines;
    std::vector<CachedAuthoringVertexDraw> authoringVertices;
    std::vector<CachedAuthoringFaceHighlightDraw> authoringFaceHighlights;
    std::vector<CachedAuthoringDiagnosticDraw> authoringDiagnostics;
};

} // namespace game
