#pragma once

#include "sector_demo/SectorTopologyTypes.h"
#include "sector_demo/SectorTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace game {

struct SectorTopologyMap {
    std::unordered_map<std::string, SectorTextureDefinition> texturesById;
    std::vector<SectorTopologyVertex> vertices;
    std::vector<SectorTopologyLineDef> lineDefs;
    std::vector<SectorTopologySideDef> sideDefs;
    std::vector<SectorTopologySector> sectors;
};

bool IsValidSectorTopologyId(int id);
const char* SectorTopologySideKindName(SectorTopologySideKind side);
SectorTopologySideKind OppositeSectorTopologySideKind(SectorTopologySideKind side);

int AllocateSectorTopologyVertexId(const SectorTopologyMap& map);
int AllocateSectorTopologyLineDefId(const SectorTopologyMap& map);
int AllocateSectorTopologySideDefId(const SectorTopologyMap& map);
int AllocateSectorTopologySectorId(const SectorTopologyMap& map);

const SectorTopologyVertex* FindSectorTopologyVertex(const SectorTopologyMap& map, int id);
SectorTopologyVertex* FindSectorTopologyVertex(SectorTopologyMap& map, int id);

const SectorTopologyLineDef* FindSectorTopologyLineDef(const SectorTopologyMap& map, int id);
SectorTopologyLineDef* FindSectorTopologyLineDef(SectorTopologyMap& map, int id);

const SectorTopologySideDef* FindSectorTopologySideDef(const SectorTopologyMap& map, int id);
SectorTopologySideDef* FindSectorTopologySideDef(SectorTopologyMap& map, int id);

const SectorTopologySector* FindSectorTopologySector(const SectorTopologyMap& map, int id);
SectorTopologySector* FindSectorTopologySector(SectorTopologyMap& map, int id);

const SectorTopologySideDef* FindOppositeSectorTopologySideDef(
        const SectorTopologyMap& map,
        int sideDefId);

bool GetSectorTopologyLineVertices(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& line,
        const SectorTopologyVertex*& outStart,
        const SectorTopologyVertex*& outEnd);

} // namespace game
