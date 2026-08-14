#include "game/navigation/SectorNavigationTypes.h"

#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>

namespace game {

bool operator==(SectorNavigationAgentHandle lhs, SectorNavigationAgentHandle rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

bool operator==(SectorNavigationPathHandle lhs, SectorNavigationPathHandle rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

bool IsNull(SectorNavigationAgentHandle handle)
{
    return handle.index == UINT32_MAX;
}

bool IsNull(SectorNavigationPathHandle handle)
{
    return handle.index == UINT32_MAX;
}

SectorNavigationSettings NormalizeSectorNavigationSettings(
        SectorNavigationSettings settings)
{
    const SectorNavigationSettings defaults;
    const auto finiteOr = [](float value, float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    settings.agentRadius = std::clamp(
            finiteOr(settings.agentRadius, defaults.agentRadius), 0.01f, 16.0f);
    settings.agentHeight = std::clamp(
            finiteOr(settings.agentHeight, defaults.agentHeight), 0.05f, 32.0f);
    settings.agentMaximumClimb = std::clamp(
            finiteOr(settings.agentMaximumClimb, defaults.agentMaximumClimb),
            0.0f,
            8.0f);
    settings.agentMaximumSlopeDegrees = std::clamp(
            finiteOr(settings.agentMaximumSlopeDegrees,
                    defaults.agentMaximumSlopeDegrees),
            0.0f,
            89.0f);
    settings.cellSize = std::clamp(
            finiteOr(settings.cellSize, defaults.cellSize), 0.01f, 4.0f);
    settings.cellHeight = std::clamp(
            finiteOr(settings.cellHeight, defaults.cellHeight), 0.01f, 4.0f);
    settings.tileSizeCells = std::clamp(settings.tileSizeCells, 8, 255);
    settings.boundsPaddingWorld = std::clamp(
            finiteOr(settings.boundsPaddingWorld, defaults.boundsPaddingWorld),
            0.0f,
            64.0f);
    settings.minimumRegionSizeCells = std::clamp(
            settings.minimumRegionSizeCells, 0, 1024);
    settings.mergeRegionSizeCells = std::clamp(
            settings.mergeRegionSizeCells, 0, 1024);
    settings.maximumEdgeLengthWorld = std::clamp(
            finiteOr(settings.maximumEdgeLengthWorld,
                    defaults.maximumEdgeLengthWorld),
            0.0f,
            1024.0f);
    settings.maximumSimplificationErrorCells = std::clamp(
            finiteOr(settings.maximumSimplificationErrorCells,
                    defaults.maximumSimplificationErrorCells),
            0.0f,
            64.0f);
    settings.maximumVerticesPerPolygon = std::clamp(
            settings.maximumVerticesPerPolygon, 3, 6);
    return settings;
}

SectorNavigationCapacitySettings NormalizeSectorNavigationCapacitySettings(
        SectorNavigationCapacitySettings settings)
{
    settings.agentCapacity = std::max<size_t>(1, settings.agentCapacity);
    settings.pathRecordCapacity = std::max<size_t>(1, settings.pathRecordCapacity);
    settings.diagnosticCapacity = std::max<size_t>(1, settings.diagnosticCapacity);
    settings.queryNodeCapacity = std::clamp(settings.queryNodeCapacity, 32, 65536);
    settings.maximumPathPolygons = std::clamp(
            settings.maximumPathPolygons,
            1,
            static_cast<int>(SectorNavigationMaximumPathPolygons));
    settings.maximumStraightPathCorners = std::clamp(
            settings.maximumStraightPathCorners,
            1,
            static_cast<int>(SectorNavigationMaximumStraightPathCorners));
    settings.tileBuildBudgetPerUpdate = std::clamp(
            settings.tileBuildBudgetPerUpdate, 1, 1024);
    settings.maximumLayersPerTileCoordinate = std::clamp(
            settings.maximumLayersPerTileCoordinate, 1, 255);
    settings.maximumTotalTiles = std::clamp(
            settings.maximumTotalTiles, 1, 1 << 22);
    settings.plannedMaximumPolygonsPerTile = std::clamp(
            settings.plannedMaximumPolygonsPerTile, 16, 1 << 20);
    settings.maximumCandidateTrianglesPerTile = std::clamp(
            settings.maximumCandidateTrianglesPerTile, 1, 1 << 24);
    settings.tileCacheTemporaryBytes = std::clamp<size_t>(
            settings.tileCacheTemporaryBytes, 64u * 1024u, 256u * 1024u * 1024u);
    settings.dynamicObstacleCapacity = std::clamp(
            settings.dynamicObstacleCapacity, 1, 65535);
    return settings;
}

SectorNavigationPosition SectorWorldToNavigationPosition(Vector3 position)
{
    return SectorNavigationPosition{{position.x, position.y, position.z}};
}

Vector3 SectorNavigationToWorldPosition(SectorNavigationPosition position)
{
    return Vector3{position.value[0], position.value[1], position.value[2]};
}

float SectorNavigationAuthoredHeightToWorld(float authoredHeight)
{
    return SectorAuthoringToWorldDistance(authoredHeight);
}

const char* SectorNavigationStateName(SectorNavigationState state)
{
    switch (state) {
        case SectorNavigationState::Uninitialized: return "uninitialized";
        case SectorNavigationState::Empty: return "empty";
        case SectorNavigationState::Queued: return "queued";
        case SectorNavigationState::Building: return "building";
        case SectorNavigationState::Ready: return "ready";
        case SectorNavigationState::Stale: return "stale";
        case SectorNavigationState::Failed: return "failed";
    }
    return "uninitialized";
}

const char* SectorNavigationQueryStatusName(SectorNavigationQueryStatus status)
{
    switch (status) {
        case SectorNavigationQueryStatus::Success: return "success";
        case SectorNavigationQueryStatus::Partial: return "partial";
        case SectorNavigationQueryStatus::StartNotOnNavmesh: return "start not on navmesh";
        case SectorNavigationQueryStatus::DestinationNotOnNavmesh: return "destination not on navmesh";
        case SectorNavigationQueryStatus::NoPath: return "no path";
        case SectorNavigationQueryStatus::CapacityExceeded: return "capacity exceeded";
        case SectorNavigationQueryStatus::NavigationUnavailable: return "navigation unavailable";
        case SectorNavigationQueryStatus::InvalidAgent: return "invalid agent";
        case SectorNavigationQueryStatus::Cancelled: return "cancelled";
        case SectorNavigationQueryStatus::Stalled: return "stalled";
        case SectorNavigationQueryStatus::TargetRemoved: return "target removed";
        case SectorNavigationQueryStatus::InternalError: return "internal error";
    }
    return "internal error";
}

const char* SectorNavigationBuildStageName(SectorNavigationBuildStage stage)
{
    switch (stage) {
        case SectorNavigationBuildStage::None: return "none";
        case SectorNavigationBuildStage::WaitingForStaticCollision: return "waiting for static collision";
        case SectorNavigationBuildStage::BuildingInput: return "building input";
        case SectorNavigationBuildStage::CalculatingCapacity: return "calculating capacity";
        case SectorNavigationBuildStage::RasterizingTiles: return "rasterizing tiles";
        case SectorNavigationBuildStage::BuildingDetourTiles: return "building Detour tiles";
        case SectorNavigationBuildStage::BuildingDebugCache: return "building debug cache";
        case SectorNavigationBuildStage::Complete: return "complete";
    }
    return "none";
}

const char* SectorNavigationDoorLinkStateName(SectorNavigationDoorLinkState state)
{
    switch (state) {
        case SectorNavigationDoorLinkState::RequiresOpening: return "Requires opening";
        case SectorNavigationDoorLinkState::Clear: return "Clear";
        case SectorNavigationDoorLinkState::Disabled: return "Disabled";
    }
    return "Unknown";
}

const char* SectorNavigationDoorDirectionName(SectorNavigationDoorDirection direction)
{
    switch (direction) {
        case SectorNavigationDoorDirection::None: return "none";
        case SectorNavigationDoorDirection::FrontToBack: return "front -> back";
        case SectorNavigationDoorDirection::BackToFront: return "back -> front";
    }
    return "unknown";
}

} // namespace game
