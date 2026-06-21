#pragma once

#include "engine/assets/TextureLoadFlags.h"
#include "sector_demo/SectorTypes.h"

namespace game {

const SectorTextureDefinition* FindSectorTexture(const SectorMap& map, const std::string& id);
std::vector<std::string> SortedSectorTextureIds(const SectorMap& map);
engine::TextureLoadFlags SectorTextureLoadFlags(SectorTextureFilter filter);
const char* SectorTextureFilterName(SectorTextureFilter filter);
bool LoadSectorMap(const char* path, SectorMap& outMap, std::string* outError = nullptr);
bool SaveSectorMap(const char* path, const SectorMap& map);
const std::vector<SectorPoint>* GetSectorBoundaryRing(
        const SectorMap& map,
        const SectorBoundaryEdgeRef& edge);
std::vector<SectorPoint>* GetSectorBoundaryRing(
        SectorMap& map,
        const SectorBoundaryEdgeRef& edge);
bool GetSectorBoundaryEdge(
        const SectorMap& map,
        const SectorBoundaryEdgeRef& edge,
        SectorPoint& outA,
        SectorPoint& outB);
EffectiveEdgeSettings GetEffectiveEdgeSettings(const SectorMap& map, const SectorBoundaryEdgeRef& edge);
EffectiveEdgeSettings GetEffectiveEdgeSettings(const SectorDefinition& sector, int edgeIndex);
EdgeNeighborInfo FindReverseEdgeNeighbor(const SectorMap& map, int sectorIndex, int edgeIndex);
EdgeNeighborInfo FindReverseEdgeNeighbor(const SectorMap& map, const SectorBoundaryEdgeRef& edge);
bool SplitSectorEdge(
        SectorMap& map,
        const SectorBoundaryEdgeRef& edge,
        int& outNewEdgeIndex,
        std::string& outError);
bool SplitSectorEdge(
        SectorMap& map,
        int sectorIndex,
        int edgeIndex,
        int& outNewEdgeIndex,
        std::string& outError);

} // namespace game
