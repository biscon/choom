#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <limits>

namespace game {
namespace {

template<typename T>
int AllocateNextId(const std::vector<T>& values)
{
    int maxId = 0;
    for (const T& value : values) {
        if (value.id > maxId) {
            maxId = value.id;
        }
    }

    if (maxId == std::numeric_limits<int>::max()) {
        return -1;
    }
    return maxId + 1;
}

template<typename T>
const T* FindById(const std::vector<T>& values, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return nullptr;
    }

    for (const T& value : values) {
        if (value.id == id) {
            return &value;
        }
    }
    return nullptr;
}

template<typename T>
T* FindById(std::vector<T>& values, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return nullptr;
    }

    for (T& value : values) {
        if (value.id == id) {
            return &value;
        }
    }
    return nullptr;
}

} // namespace

SectorTopologyIndexes BuildSectorTopologyIndexes(const SectorTopologyMap& map)
{
    SectorTopologyIndexes indexes;

    for (size_t i = 0; i < map.vertices.size(); ++i) {
        indexes.vertexIndicesById[map.vertices[i].id].push_back(i);
    }
    for (size_t i = 0; i < map.lineDefs.size(); ++i) {
        indexes.lineDefIndicesById[map.lineDefs[i].id].push_back(i);
    }
    for (size_t i = 0; i < map.sideDefs.size(); ++i) {
        const SectorTopologySideDef& sideDef = map.sideDefs[i];
        indexes.sideDefIndicesById[sideDef.id].push_back(i);
        indexes.sideDefIndicesBySectorId[sideDef.sectorId].push_back(i);
        if (sideDef.side == SectorTopologySideKind::Front) {
            indexes.frontSideDefIndicesByLineDefId[sideDef.lineDefId].push_back(i);
        } else if (sideDef.side == SectorTopologySideKind::Back) {
            indexes.backSideDefIndicesByLineDefId[sideDef.lineDefId].push_back(i);
        }
    }
    for (size_t i = 0; i < map.sectors.size(); ++i) {
        indexes.sectorIndicesById[map.sectors[i].id].push_back(i);
    }

    return indexes;
}

bool IsValidSectorTopologyId(int id)
{
    return id > 0;
}

const char* SectorTopologySideKindName(SectorTopologySideKind side)
{
    switch (side) {
        case SectorTopologySideKind::Front:
            return "Front";
        case SectorTopologySideKind::Back:
            return "Back";
    }
    return "Unknown";
}

SectorTopologySideKind OppositeSectorTopologySideKind(SectorTopologySideKind side)
{
    return side == SectorTopologySideKind::Front
           ? SectorTopologySideKind::Back
           : SectorTopologySideKind::Front;
}

int AllocateSectorTopologyVertexId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.vertices);
}

int AllocateSectorTopologyLineDefId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.lineDefs);
}

int AllocateSectorTopologySideDefId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.sideDefs);
}

int AllocateSectorTopologySectorId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.sectors);
}

int AllocateSectorTopologyStaticLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.staticLights);
}

const SectorTopologyVertex* FindSectorTopologyVertex(const SectorTopologyMap& map, int id)
{
    return FindById(map.vertices, id);
}

SectorTopologyVertex* FindSectorTopologyVertex(SectorTopologyMap& map, int id)
{
    return FindById(map.vertices, id);
}

const SectorTopologyLineDef* FindSectorTopologyLineDef(const SectorTopologyMap& map, int id)
{
    return FindById(map.lineDefs, id);
}

SectorTopologyLineDef* FindSectorTopologyLineDef(SectorTopologyMap& map, int id)
{
    return FindById(map.lineDefs, id);
}

const SectorTopologySideDef* FindSectorTopologySideDef(const SectorTopologyMap& map, int id)
{
    return FindById(map.sideDefs, id);
}

SectorTopologySideDef* FindSectorTopologySideDef(SectorTopologyMap& map, int id)
{
    return FindById(map.sideDefs, id);
}

const SectorTopologySector* FindSectorTopologySector(const SectorTopologyMap& map, int id)
{
    return FindById(map.sectors, id);
}

SectorTopologySector* FindSectorTopologySector(SectorTopologyMap& map, int id)
{
    return FindById(map.sectors, id);
}

const SectorTopologyStaticPointLight* FindSectorTopologyStaticLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.staticLights, id);
}

SectorTopologyStaticPointLight* FindSectorTopologyStaticLight(SectorTopologyMap& map, int id)
{
    return FindById(map.staticLights, id);
}

bool RemoveSectorTopologyStaticLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.staticLights.begin(),
            map.staticLights.end(),
            [id](const SectorTopologyStaticPointLight& light) { return light.id == id; });
    if (found == map.staticLights.end()) {
        return false;
    }

    map.staticLights.erase(found);
    return true;
}

const SectorTopologySideDef* FindOppositeSectorTopologySideDef(
        const SectorTopologyMap& map,
        int sideDefId)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef->lineDefId);
    if (lineDef == nullptr) {
        return nullptr;
    }

    int oppositeSideDefId = -1;
    SectorTopologySideKind expectedOppositeKind = SectorTopologySideKind::Front;
    if (sideDef->side == SectorTopologySideKind::Front) {
        if (lineDef->frontSideDefId != sideDef->id) {
            return nullptr;
        }
        oppositeSideDefId = lineDef->backSideDefId;
        expectedOppositeKind = SectorTopologySideKind::Back;
    } else if (sideDef->side == SectorTopologySideKind::Back) {
        if (lineDef->backSideDefId != sideDef->id) {
            return nullptr;
        }
        oppositeSideDefId = lineDef->frontSideDefId;
        expectedOppositeKind = SectorTopologySideKind::Front;
    } else {
        return nullptr;
    }

    const SectorTopologySideDef* opposite = FindSectorTopologySideDef(map, oppositeSideDefId);
    if (opposite == nullptr
        || opposite->lineDefId != lineDef->id
        || opposite->side != expectedOppositeKind) {
        return nullptr;
    }
    return opposite;
}

bool GetSectorTopologyLineVertices(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& line,
        const SectorTopologyVertex*& outStart,
        const SectorTopologyVertex*& outEnd)
{
    outStart = nullptr;
    outEnd = nullptr;

    const SectorTopologyVertex* start = FindSectorTopologyVertex(map, line.startVertexId);
    const SectorTopologyVertex* end = FindSectorTopologyVertex(map, line.endVertexId);
    if (start == nullptr || end == nullptr) {
        return false;
    }

    outStart = start;
    outEnd = end;
    return true;
}

} // namespace game
