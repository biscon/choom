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
    float eyeHeight = 5.0f;
};

struct SectorTopologyMap {
    std::unordered_map<std::string, SectorTextureDefinition> texturesById;
    std::vector<SectorTopologyVertex> vertices;
    std::vector<SectorTopologyLineDef> lineDefs;
    std::vector<SectorTopologySideDef> sideDefs;
    std::vector<SectorTopologySector> sectors;
    std::vector<SectorTopologyStaticPointLight> staticLights;
    SectorPreviewSettings previewSettings;
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

bool IsValidSectorTopologyId(int id);
const char* SectorTopologySideKindName(SectorTopologySideKind side);
SectorTopologySideKind OppositeSectorTopologySideKind(SectorTopologySideKind side);

int AllocateSectorTopologyVertexId(const SectorTopologyMap& map);
int AllocateSectorTopologyLineDefId(const SectorTopologyMap& map);
int AllocateSectorTopologySideDefId(const SectorTopologyMap& map);
int AllocateSectorTopologySectorId(const SectorTopologyMap& map);
int AllocateSectorTopologyStaticLightId(const SectorTopologyMap& map);

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

std::vector<SectorTopologyValidationIssue> ValidateSectorTopologyMap(
        const SectorTopologyMap& map);

bool HasSectorTopologyValidationErrors(
        const std::vector<SectorTopologyValidationIssue>& issues);

std::string FormatSectorTopologyValidationIssue(
        const SectorTopologyValidationIssue& issue);

} // namespace game
