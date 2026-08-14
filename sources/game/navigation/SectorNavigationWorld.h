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
            SectorNavigationCapacitySettings capacities = {});
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

    SectorNavigationNearestPointResult FindNearestPoint(Vector3 position) const;
    SectorNavigationPathResult FindPath(Vector3 start, Vector3 destination);

    SectorNavigationAgentHandle AllocateAgentRecord();
    SectorNavigationPathHandle AllocatePathRecord();
    bool ReleaseAgentRecord(SectorNavigationAgentHandle handle);
    bool ReleasePathRecord(SectorNavigationPathHandle handle);

    SectorNavigationState State() const;
    SectorNavigationBuildStage BuildStage() const;
    const SectorNavigationSettings& Settings() const;
    const SectorNavigationCapacitySettings& Capacities() const;
    const SectorNavigationCounters& Counters() const;
    const std::vector<SectorNavigationDiagnostic>& Diagnostics() const;
    uint64_t SourceRevision() const;
    uint64_t BuildRevision() const;
    uint64_t SourceHash() const;
    const SectorNavigationBuildStatistics& BuildStatistics() const;
    const SectorNavigationDebugCache& DebugCache() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace game
