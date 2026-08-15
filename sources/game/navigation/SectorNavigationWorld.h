#pragma once

#include "game/navigation/SectorNavigationTypes.h"

#include <memory>
#include <vector>

namespace game {

struct SectorStaticModelCollider;
struct SectorTopologyMap;

class SectorNavigationWorld {
public:
    SectorNavigationWorld();
    ~SectorNavigationWorld();

    SectorNavigationWorld(const SectorNavigationWorld&) = delete;
    SectorNavigationWorld& operator=(const SectorNavigationWorld&) = delete;
    SectorNavigationWorld(SectorNavigationWorld&&) noexcept;
    SectorNavigationWorld& operator=(SectorNavigationWorld&&) noexcept;

    bool Initialize(
            SectorNavigationSettings settings = {},
            SectorNavigationCapacitySettings capacities = {},
            SectorNavigationDynamicObstacleSettings dynamicObstacleSettings = {},
            SectorNavigationCrowdSettings crowdSettings = {});
    void Shutdown();
    void ResetForRebuild();
    void RequestRebuild();
    void MarkStale();
    void SetEmpty();
    void Fail(
            SectorNavigationBuildStage stage,
            const std::string& message);

    // Performs build work only while a rebuild is explicitly queued/building.
    // Normal ready-state updates return immediately.
    void UpdateBuild(
            const SectorTopologyMap& map,
            const std::vector<SectorStaticModelCollider>& staticColliders,
            size_t pendingStaticColliderCount);

    // Reconciles the current collision-enabled dynamic prop set and performs
    // bounded TileCache request/tile work. Call during an explicit runtime
    // phase before path/corridor updates.
    void UpdateDynamicObstacles(
            const std::vector<SectorStaticModelCollider>& dynamicColliders,
            float dt);
    bool CorridorTouchesChangedTile(
            const SectorNavigationTileKey* tiles,
            size_t tileCount,
            uint64_t capturedTileRevision) const;

    SectorNavigationNearestPointResult FindNearestPoint(Vector3 position) const;
    SectorNavigationPathResult FindPath(
            Vector3 start,
            Vector3 destination,
            SectorNavigationQueryOptions options = {});

    bool SetDoorLinkRuntimeState(
            int placedObjectId,
            SectorNavigationDoorLinkState state,
            uint32_t holderCount);
    bool GetDoorLinkRuntimeState(
            int placedObjectId,
            SectorNavigationDoorLinkState& outState) const;

    SectorNavigationAgentHandle AllocateAgentRecord();
    SectorNavigationPathHandle AllocatePathRecord();
    bool IsAgentRecordValid(SectorNavigationAgentHandle handle) const;
    bool IsPathRecordValid(SectorNavigationPathHandle handle) const;
    bool ReleaseAgentRecord(SectorNavigationAgentHandle handle);
    bool ReleasePathRecord(SectorNavigationPathHandle handle);

    bool SynchronizeCrowdAgent(
            SectorNavigationAgentHandle handle,
            Vector3 physicalPosition,
            Vector2 actualVelocity,
            float maximumSpeed,
            bool participate = true);
    bool SetCrowdAgentDesiredVelocity(
            SectorNavigationAgentHandle handle,
            Vector2 desiredVelocity);
    void UpdateCrowd(float dt);
    SectorNavigationCrowdAgentState GetCrowdAgentState(
            SectorNavigationAgentHandle handle) const;

    SectorNavigationState State() const;
    SectorNavigationBuildStage BuildStage() const;
    const SectorNavigationSettings& Settings() const;
    const SectorNavigationCapacitySettings& Capacities() const;
    const SectorNavigationDynamicObstacleSettings& DynamicObstacleSettings() const;
    const SectorNavigationDynamicObstacleStatistics& DynamicObstacleStatistics() const;
    const SectorNavigationCrowdSettings& CrowdSettings() const;
    const SectorNavigationCrowdStatistics& CrowdStatistics() const;
    const SectorNavigationCounters& Counters() const;
    const std::vector<SectorNavigationDiagnostic>& Diagnostics() const;
    uint64_t SourceRevision() const;
    uint64_t BuildRevision() const;
    uint64_t TileRevision() const;
    uint64_t SourceHash() const;
    const SectorNavigationBuildStatistics& BuildStatistics() const;
    const SectorNavigationDebugCache& DebugCache() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace game
