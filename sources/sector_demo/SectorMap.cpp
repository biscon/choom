#include "sector_demo/SectorMap.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game {

namespace {

constexpr float GeometryEpsilon = 0.001f;

bool SamePoint(SectorPoint a, SectorPoint b)
{
    return std::fabs(a.x - b.x) <= GeometryEpsilon && std::fabs(a.y - b.y) <= GeometryEpsilon;
}

bool SameBoundaryEdge(const SectorEdgeOverride& edgeOverride, SectorBoundaryRingKind ringKind, int holeIndex, int edgeIndex)
{
    return edgeOverride.ringKind == ringKind
            && edgeOverride.holeIndex == (ringKind == SectorBoundaryRingKind::Hole ? holeIndex : -1)
            && edgeOverride.edgeIndex == edgeIndex;
}

const SectorEdgeOverride* FindEdgeOverride(
        const SectorDefinition& sector,
        SectorBoundaryRingKind ringKind,
        int holeIndex,
        int edgeIndex)
{
    for (const SectorEdgeOverride& edgeOverride : sector.edgeOverrides) {
        if (SameBoundaryEdge(edgeOverride, ringKind, holeIndex, edgeIndex)) {
            return &edgeOverride;
        }
    }
    return nullptr;
}

void ApplyPartUvOverride(EffectiveEdgePartSettings& settings, const SectorEdgePartUvOverride& uv)
{
    if (uv.hasUvScale) {
        settings.uvScale = uv.uvScale;
    }
    if (uv.hasUvOffset) {
        settings.uvOffset = uv.uvOffset;
    }
}

void InsertSectorEdgeMidpoint(
        SectorDefinition& sector,
        SectorBoundaryRingKind ringKind,
        int holeIndex,
        int edgeIndex,
        SectorPoint midpoint)
{
    std::vector<SectorEdgeOverride> remappedOverrides;
    remappedOverrides.reserve(sector.edgeOverrides.size() + 1);
    for (const SectorEdgeOverride& oldOverride : sector.edgeOverrides) {
        SectorEdgeOverride remapped = oldOverride;
        const bool sameRing = oldOverride.ringKind == ringKind
                && oldOverride.holeIndex == (ringKind == SectorBoundaryRingKind::Hole ? holeIndex : -1);
        if (sameRing && oldOverride.edgeIndex > edgeIndex) {
            ++remapped.edgeIndex;
        }
        remappedOverrides.push_back(std::move(remapped));

        if (sameRing && oldOverride.edgeIndex == edgeIndex) {
            SectorEdgeOverride secondHalf = oldOverride;
            secondHalf.edgeIndex = edgeIndex + 1;
            remappedOverrides.push_back(std::move(secondHalf));
        }
    }

    std::vector<SectorPoint>& ring = ringKind == SectorBoundaryRingKind::Outer
            ? sector.points : sector.holes[static_cast<size_t>(holeIndex)];
    ring.insert(ring.begin() + edgeIndex + 1, midpoint);
    sector.edgeOverrides = std::move(remappedOverrides);
}

} // namespace

const SectorTextureDefinition* FindSectorTexture(const SectorMap& map, const std::string& id)
{
    if (id.empty()) {
        return nullptr;
    }

    const auto it = map.texturesById.find(id);
    return it == map.texturesById.end() ? nullptr : &it->second;
}

std::vector<std::string> SortedSectorTextureIds(const SectorMap& map)
{
    std::vector<std::string> ids;
    ids.reserve(map.texturesById.size());
    for (const auto& texture : map.texturesById) {
        ids.push_back(texture.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

EffectiveEdgeSettings GetEffectiveEdgeSettings(const SectorDefinition& sector, int edgeIndex)
{
    EffectiveEdgeSettings settings;
    settings.wall.textureId = sector.wallTextureId;
    settings.lower.textureId = sector.lowerWallTextureId;
    settings.upper.textureId = sector.upperWallTextureId;

    const SectorEdgeOverride* edgeOverride = FindEdgeOverride(
            sector, SectorBoundaryRingKind::Outer, -1, edgeIndex);
    if (edgeOverride == nullptr) {
        return settings;
    }

    if (edgeOverride->hasWallTexture) {
        settings.wall.textureId = edgeOverride->wallTextureId;
    }
    if (edgeOverride->hasLowerWallTexture) {
        settings.lower.textureId = edgeOverride->lowerWallTextureId;
    }
    if (edgeOverride->hasUpperWallTexture) {
        settings.upper.textureId = edgeOverride->upperWallTextureId;
    }
    ApplyPartUvOverride(settings.wall, edgeOverride->wallUv);
    ApplyPartUvOverride(settings.lower, edgeOverride->lowerUv);
    ApplyPartUvOverride(settings.upper, edgeOverride->upperUv);

    return settings;
}

EffectiveEdgeSettings GetEffectiveEdgeSettings(const SectorMap& map, const SectorBoundaryEdgeRef& edge)
{
    EffectiveEdgeSettings settings;
    if (edge.sectorIndex < 0 || edge.sectorIndex >= static_cast<int>(map.sectors.size())) {
        return settings;
    }

    const SectorDefinition& sector = map.sectors[static_cast<size_t>(edge.sectorIndex)];
    settings.wall.textureId = sector.wallTextureId;
    settings.lower.textureId = sector.lowerWallTextureId;
    settings.upper.textureId = sector.upperWallTextureId;

    const std::vector<SectorPoint>* ring = GetSectorBoundaryRing(map, edge);
    if (ring == nullptr || edge.edgeIndex < 0 || edge.edgeIndex >= static_cast<int>(ring->size())) {
        return settings;
    }
    const SectorEdgeOverride* edgeOverride = FindEdgeOverride(
            sector, edge.ringKind, edge.holeIndex, edge.edgeIndex);
    if (edgeOverride == nullptr) {
        return settings;
    }
    if (edgeOverride->hasWallTexture) settings.wall.textureId = edgeOverride->wallTextureId;
    if (edgeOverride->hasLowerWallTexture) settings.lower.textureId = edgeOverride->lowerWallTextureId;
    if (edgeOverride->hasUpperWallTexture) settings.upper.textureId = edgeOverride->upperWallTextureId;
    ApplyPartUvOverride(settings.wall, edgeOverride->wallUv);
    ApplyPartUvOverride(settings.lower, edgeOverride->lowerUv);
    ApplyPartUvOverride(settings.upper, edgeOverride->upperUv);
    return settings;
}

EdgeNeighborInfo FindReverseEdgeNeighbor(const SectorMap& map, int sectorIndex, int edgeIndex)
{
    return FindReverseEdgeNeighbor(
            map,
            SectorBoundaryEdgeRef{sectorIndex, SectorBoundaryRingKind::Outer, -1, edgeIndex}
    );
}

const std::vector<SectorPoint>* GetSectorBoundaryRing(
        const SectorMap& map,
        const SectorBoundaryEdgeRef& edge)
{
    if (edge.sectorIndex < 0 || edge.sectorIndex >= static_cast<int>(map.sectors.size())) {
        return nullptr;
    }
    const SectorDefinition& sector = map.sectors[static_cast<size_t>(edge.sectorIndex)];
    if (edge.ringKind == SectorBoundaryRingKind::Outer) {
        return &sector.points;
    }
    if (edge.holeIndex < 0 || edge.holeIndex >= static_cast<int>(sector.holes.size())) {
        return nullptr;
    }
    return &sector.holes[static_cast<size_t>(edge.holeIndex)];
}

std::vector<SectorPoint>* GetSectorBoundaryRing(
        SectorMap& map,
        const SectorBoundaryEdgeRef& edge)
{
    return const_cast<std::vector<SectorPoint>*>(GetSectorBoundaryRing(
            static_cast<const SectorMap&>(map), edge));
}

bool GetSectorBoundaryEdge(
        const SectorMap& map,
        const SectorBoundaryEdgeRef& edge,
        SectorPoint& outA,
        SectorPoint& outB)
{
    const std::vector<SectorPoint>* ring = GetSectorBoundaryRing(map, edge);
    if (ring == nullptr || ring->size() < 2
            || edge.edgeIndex < 0 || edge.edgeIndex >= static_cast<int>(ring->size())) {
        return false;
    }
    outA = (*ring)[static_cast<size_t>(edge.edgeIndex)];
    outB = (*ring)[(static_cast<size_t>(edge.edgeIndex) + 1) % ring->size()];
    return true;
}

EdgeNeighborInfo FindReverseEdgeNeighbor(const SectorMap& map, const SectorBoundaryEdgeRef& edge)
{
    SectorPoint a{};
    SectorPoint b{};
    if (!GetSectorBoundaryEdge(map, edge, a, b)) {
        return EdgeNeighborInfo{};
    }
    for (size_t otherSectorIndex = 0; otherSectorIndex < map.sectors.size(); ++otherSectorIndex) {
        if (static_cast<int>(otherSectorIndex) == edge.sectorIndex) {
            continue;
        }
        const SectorDefinition& other = map.sectors[otherSectorIndex];
        const size_t ringCount = 1 + other.holes.size();
        for (size_t ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
            const std::vector<SectorPoint>& ring = ringIndex == 0
                    ? other.points : other.holes[ringIndex - 1];
            for (size_t otherEdgeIndex = 0; otherEdgeIndex < ring.size(); ++otherEdgeIndex) {
                const SectorPoint otherA = ring[otherEdgeIndex];
                const SectorPoint otherB = ring[(otherEdgeIndex + 1) % ring.size()];
                if (SamePoint(a, otherB) && SamePoint(b, otherA)) {
                    return EdgeNeighborInfo{
                            true,
                            static_cast<int>(otherSectorIndex),
                            static_cast<int>(otherEdgeIndex),
                            ringIndex == 0 ? SectorBoundaryRingKind::Outer : SectorBoundaryRingKind::Hole,
                            ringIndex == 0 ? -1 : static_cast<int>(ringIndex - 1)
                    };
                }
            }
        }
    }

    return EdgeNeighborInfo{};
}

bool SplitSectorEdge(
        SectorMap& map,
        const SectorBoundaryEdgeRef& edge,
        int& outNewEdgeIndex,
        std::string& outError)
{
    outNewEdgeIndex = -1;
    outError.clear();
    const std::vector<SectorPoint>* selectedRing = GetSectorBoundaryRing(map, edge);
    if (selectedRing == nullptr || selectedRing->size() < 3) {
        outError = "Cannot split edge: invalid boundary ring";
        return false;
    }
    if (edge.edgeIndex < 0 || edge.edgeIndex >= static_cast<int>(selectedRing->size())) {
        outError = "Cannot split edge: invalid edge index";
        return false;
    }

    const SectorPoint a = (*selectedRing)[static_cast<size_t>(edge.edgeIndex)];
    const SectorPoint b = (*selectedRing)[(static_cast<size_t>(edge.edgeIndex) + 1) % selectedRing->size()];
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    if (dx * dx + dy * dy <= GeometryEpsilon * GeometryEpsilon) {
        outError = "Cannot split edge: edge length is effectively zero";
        return false;
    }
    const SectorPoint midpoint{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
    if (SamePoint(midpoint, a) || SamePoint(midpoint, b)) {
        outError = "Cannot split edge: midpoint is too close to an endpoint";
        return false;
    }

    const EdgeNeighborInfo neighbor = FindReverseEdgeNeighbor(map, edge);
    if (neighbor.hasNeighbor) {
        const SectorBoundaryEdgeRef neighborRef{
                neighbor.sectorIndex, neighbor.ringKind, neighbor.holeIndex, neighbor.edgeIndex};
        const std::vector<SectorPoint>* neighborRing = GetSectorBoundaryRing(map, neighborRef);
        if (neighborRing == nullptr || neighbor.edgeIndex < 0
                || neighbor.edgeIndex >= static_cast<int>(neighborRing->size())) {
            outError = "Cannot split edge: invalid reverse boundary edge";
            return false;
        }
    }

    InsertSectorEdgeMidpoint(
            map.sectors[static_cast<size_t>(edge.sectorIndex)],
            edge.ringKind, edge.holeIndex, edge.edgeIndex, midpoint);
    if (neighbor.hasNeighbor) {
        InsertSectorEdgeMidpoint(
                map.sectors[static_cast<size_t>(neighbor.sectorIndex)],
                neighbor.ringKind, neighbor.holeIndex, neighbor.edgeIndex, midpoint);
    }
    outNewEdgeIndex = edge.edgeIndex + 1;
    return true;
}

bool SplitSectorEdge(
        SectorMap& map,
        int sectorIndex,
        int edgeIndex,
        int& outNewEdgeIndex,
        std::string& outError)
{
    return SplitSectorEdge(
            map,
            SectorBoundaryEdgeRef{sectorIndex, SectorBoundaryRingKind::Outer, -1, edgeIndex},
            outNewEdgeIndex,
            outError);
}

} // namespace game
