#pragma once

#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTopologyMap.h"

#include <string>

namespace game {

struct SectorAuthoringDocument {
    SectorAuthoringGraph graph;
    SectorTopologyMap mapData;
    SectorAuthoringDerivationResult derivation;
};

bool LoadSectorTopologyMap(
        const char* path,
        SectorTopologyMap& outMap,
        std::string* outError = nullptr);

bool SaveSectorTopologyMap(
        const char* path,
        const SectorTopologyMap& map,
        std::string* outError = nullptr);

bool LoadSectorTopologyMapFromJsonString(
        const std::string& jsonText,
        SectorTopologyMap& outMap,
        std::string* outError = nullptr);

bool SaveSectorTopologyMapToJsonString(
        const SectorTopologyMap& map,
        std::string& outJsonText,
        std::string* outError = nullptr);

bool LoadSectorAuthoringDocument(
        const char* path,
        SectorAuthoringDocument& outDocument,
        std::string* outError = nullptr);

bool SaveSectorAuthoringDocument(
        const char* path,
        const SectorAuthoringDocument& document,
        std::string* outError = nullptr);

bool LoadSectorAuthoringDocumentFromJsonString(
        const std::string& jsonText,
        SectorAuthoringDocument& outDocument,
        std::string* outError = nullptr);

bool SaveSectorAuthoringDocumentToJsonString(
        const SectorAuthoringDocument& document,
        std::string& outJsonText,
        std::string* outError = nullptr);

} // namespace game
