#pragma once

#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorTopologyTypes.h"
#include "sector_demo/SectorTextureTypes.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

struct SectorPreviewSettings {
    float walkSpeed = 6.0f;
    float runSpeed = 12.0f;
    float mouseSensitivity = 1.0f;
    float eyeHeight = 1.2f;
    float gravity = 25.0f;
    float playerRadius = 0.25f;
    float playerHeight = 1.6f;
    float stepHeight = 0.25f;
    float jumpHeight = 0.6f;
    float headBobStrength = 0.020f;
    float headBobFrequency = 2.0f;
};

struct SectorTopologySkySettings {
    std::string textureId = "sky_cylinder";
    float yawOffsetDegrees = 0.0f;
    float verticalOffset = 0.0f;
    float verticalScale = 1.0f;
    Color topColor = Color{95, 165, 235, 255};
};

struct SectorTopologyDirectionalLightSettings {
    bool enabled = false;
    Vector3 directionToLight = Vector3{-0.35f, 0.80f, -0.25f};
    Color color = Color{255, 244, 214, 255};
    float intensity = 1.0f;
};

struct SectorPlacedBillboard {
    std::string spriteAnimationPath;
    Vector2 sizeWorld = {1.0f, 1.0f};
    bool keepAspectRatio = true;
    Vector2 originNormalized = {0.5f, 1.0f};
    bool directional = false;
    std::string clip;
    std::string frontClip = "Front";
    std::string backClip = "Back";
    std::string leftClip = "Left";
    std::string rightClip = "Right";
    bool playing = true;
};

struct SectorPlacedRuntimeObject {
    int id = 0;
    std::string definitionId;
    Vector3 position = {};
    float yawRadians = 0.0f;
    std::string kind;
    SectorPlacedBillboard billboard;
};

struct SectorTopologyMap {
    std::unordered_map<std::string, SectorTextureDefinition> texturesById;
    std::vector<SectorTopologyVertex> vertices;
    std::vector<SectorTopologyLineDef> lineDefs;
    std::vector<SectorTopologySideDef> sideDefs;
    std::vector<SectorTopologySector> sectors;
    std::vector<SectorTopologyStaticPointLight> staticLights;
    std::vector<SectorTopologyStaticSpotLight> staticSpotLights;
    std::vector<SectorTopologyDynamicPointLight> dynamicPointLights;
    std::vector<SectorTopologyDynamicSpotLight> dynamicSpotLights;
    std::vector<SectorPlacedRuntimeObject> runtimeObjects;
    SectorPreviewSettings previewSettings;
    SectorTopologySkySettings skySettings;
    SectorTopologyDirectionalLightSettings directionalLight;
    SectorLightmapBakeSettings lightmapSettings;
    SectorLightmapMetadata bakedLightmap;
};

// Transient lookup data. Index vectors intentionally retain duplicate IDs so
// malformed maps can be diagnosed without choosing an arbitrary object.
struct SectorTopologyIndexes {
    std::unordered_map<int, std::vector<size_t>> vertexIndicesById;
    std::unordered_map<int, std::vector<size_t>> lineDefIndicesById;
    std::unordered_map<int, std::vector<size_t>> sideDefIndicesById;
    std::unordered_map<int, std::vector<size_t>> sectorIndicesById;

    std::unordered_map<int, std::vector<size_t>> sideDefIndicesBySectorId;
    std::unordered_map<int, std::vector<size_t>> frontSideDefIndicesByLineDefId;
    std::unordered_map<int, std::vector<size_t>> backSideDefIndicesByLineDefId;
};

SectorTopologyIndexes BuildSectorTopologyIndexes(const SectorTopologyMap& map);

SectorPreviewSettings DefaultSectorPreviewSettings();
SectorPreviewSettings NormalizeSectorPreviewSettings(SectorPreviewSettings settings);
SectorTopologySkySettings DefaultSectorTopologySkySettings();
SectorTopologySkySettings NormalizeSectorTopologySkySettings(SectorTopologySkySettings settings);
SectorTopologyDirectionalLightSettings DefaultSectorTopologyDirectionalLightSettings();
SectorTopologyDirectionalLightSettings NormalizeSectorTopologyDirectionalLightSettings(
        SectorTopologyDirectionalLightSettings settings);

bool IsValidSectorTopologyId(int id);
const char* SectorTopologySideKindName(SectorTopologySideKind side);
SectorTopologySideKind OppositeSectorTopologySideKind(SectorTopologySideKind side);

int AllocateSectorTopologyVertexId(const SectorTopologyMap& map);
int AllocateSectorTopologyLineDefId(const SectorTopologyMap& map);
int AllocateSectorTopologySideDefId(const SectorTopologyMap& map);
int AllocateSectorTopologySectorId(const SectorTopologyMap& map);
int AllocateSectorTopologyStaticLightId(const SectorTopologyMap& map);
int AllocateSectorTopologyStaticSpotLightId(const SectorTopologyMap& map);
int AllocateSectorTopologyDynamicLightId(const SectorTopologyMap& map);
int AllocateSectorTopologyDynamicSpotLightId(const SectorTopologyMap& map);
int AllocateSectorPlacedRuntimeObjectId(const SectorTopologyMap& map);

const SectorTopologyVertex* FindSectorTopologyVertex(const SectorTopologyMap& map, int id);
SectorTopologyVertex* FindSectorTopologyVertex(SectorTopologyMap& map, int id);

const SectorTopologyLineDef* FindSectorTopologyLineDef(const SectorTopologyMap& map, int id);
SectorTopologyLineDef* FindSectorTopologyLineDef(SectorTopologyMap& map, int id);

const SectorTopologySideDef* FindSectorTopologySideDef(const SectorTopologyMap& map, int id);
SectorTopologySideDef* FindSectorTopologySideDef(SectorTopologyMap& map, int id);

const SectorTopologySector* FindSectorTopologySector(const SectorTopologyMap& map, int id);
SectorTopologySector* FindSectorTopologySector(SectorTopologyMap& map, int id);

const SectorTopologyStaticPointLight* FindSectorTopologyStaticLight(const SectorTopologyMap& map, int id);
SectorTopologyStaticPointLight* FindSectorTopologyStaticLight(SectorTopologyMap& map, int id);

bool RemoveSectorTopologyStaticLight(SectorTopologyMap& map, int id);

const SectorTopologyStaticSpotLight* FindSectorTopologyStaticSpotLight(const SectorTopologyMap& map, int id);
SectorTopologyStaticSpotLight* FindSectorTopologyStaticSpotLight(SectorTopologyMap& map, int id);

bool RemoveSectorTopologyStaticSpotLight(SectorTopologyMap& map, int id);

const SectorTopologyDynamicPointLight* FindSectorTopologyDynamicLight(const SectorTopologyMap& map, int id);
SectorTopologyDynamicPointLight* FindSectorTopologyDynamicLight(SectorTopologyMap& map, int id);

bool RemoveSectorTopologyDynamicLight(SectorTopologyMap& map, int id);

const SectorTopologyDynamicSpotLight* FindSectorTopologyDynamicSpotLight(const SectorTopologyMap& map, int id);
SectorTopologyDynamicSpotLight* FindSectorTopologyDynamicSpotLight(SectorTopologyMap& map, int id);

bool RemoveSectorTopologyDynamicSpotLight(SectorTopologyMap& map, int id);

const SectorPlacedRuntimeObject* FindSectorPlacedRuntimeObject(const SectorTopologyMap& map, int id);
SectorPlacedRuntimeObject* FindSectorPlacedRuntimeObject(SectorTopologyMap& map, int id);

bool RemoveSectorPlacedRuntimeObject(SectorTopologyMap& map, int id);

const SectorTopologySideDef* FindOppositeSectorTopologySideDef(
        const SectorTopologyMap& map,
        int sideDefId);

bool GetSectorTopologyLineVertices(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& line,
        const SectorTopologyVertex*& outStart,
        const SectorTopologyVertex*& outEnd);

bool ExtractSectorTopologyLoops(
        const SectorTopologyMap& map,
        int sectorId,
        SectorTopologyLoopSet& outLoops,
        std::vector<SectorTopologyValidationIssue>* outIssues = nullptr);

bool ExtractSectorTopologyLoops(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int sectorId,
        SectorTopologyLoopSet& outLoops,
        std::vector<SectorTopologyValidationIssue>* outIssues = nullptr);

std::vector<SectorTopologyValidationIssue> ValidateSectorTopologyMap(
        const SectorTopologyMap& map);

bool HasSectorTopologyValidationErrors(
        const std::vector<SectorTopologyValidationIssue>& issues);

std::string FormatSectorTopologyValidationIssue(
        const SectorTopologyValidationIssue& issue);

} // namespace game
