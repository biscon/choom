#pragma once

#include "sector_editor/SectorEditorLightmapAsyncTypes.h"

#include <optional>
#include <string>

namespace game {

struct SectorEditorLightmapBakeRequest {
    SectorTopologyMap mapSnapshot;
    std::string expectedSourceHash;
    std::string finalOutputPath;
    std::string temporaryOutputPath;
    uint64_t sourceMapRevision = 0;
};

enum class SectorEditorLightmapBakePollStatus {
    None,
    Cancelled,
    Failed,
    Completed,
};

struct SectorEditorLightmapBakePollResult {
    SectorEditorLightmapBakePollStatus status = SectorEditorLightmapBakePollStatus::None;
    std::string message;
    std::optional<SectorLightmapBakeAsyncResult> completedResult;
};

struct SectorEditorLightmapBakeInstallPayload {
    SectorLightmapBakeResult bakeResult;
    std::string finalLightmapPath;
    std::string finalObjectProbePath;
    std::string finalObjectProbeAssetPath;
    std::string status;
};

class SectorEditorLightmapBakeController {
public:
    SectorEditorLightmapBakeController();
    ~SectorEditorLightmapBakeController();

    bool CanStart() const;
    bool IsBlocking() const;
    bool HasRunningBake() const;

    const LightmapBakeProgress& Progress() const;

    bool StartBake(SectorEditorLightmapBakeRequest request, std::string& outStatus);
    SectorEditorLightmapBakePollResult Poll();
    bool IsCompletedResultStale(
            const SectorLightmapBakeAsyncResult& result,
            const std::string& currentSourceHash) const;
    bool InstallCompletedResultFiles(
            const SectorLightmapBakeAsyncResult& result,
            const std::string& currentSourceHash,
            SectorEditorLightmapBakeInstallPayload& outPayload) const;

    bool RequestCancel();
    void JoinWorker();
    void Shutdown();
    void AcknowledgeTerminalState();

    void CompleteInstall(bool installed);

    SectorEditorLightmapBakeModalView BuildModalView() const;

private:
    LightmapBakeAsyncState state_;
};

} // namespace game
