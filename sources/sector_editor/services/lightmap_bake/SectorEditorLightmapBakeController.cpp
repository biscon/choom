#include "sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorLightmapReport.h"

#include <raylib.h>

#include <filesystem>
#include <system_error>

namespace game {

SectorEditorLightmapBakeController::SectorEditorLightmapBakeController() = default;

SectorEditorLightmapBakeController::~SectorEditorLightmapBakeController()
{
    Shutdown();
}

bool SectorEditorLightmapBakeController::CanStart() const
{
    return !state_.progress.running.load() && !state_.worker.joinable() && !state_.modalOpen;
}

bool SectorEditorLightmapBakeController::IsBlocking() const
{
    return state_.modalOpen || state_.progress.running.load();
}

bool SectorEditorLightmapBakeController::HasRunningBake() const
{
    return state_.progress.running.load();
}

const LightmapBakeProgress& SectorEditorLightmapBakeController::Progress() const
{
    return state_.progress;
}

bool SectorEditorLightmapBakeController::StartBake(SectorEditorLightmapBakeRequest request, std::string& outStatus)
{
    if (!CanStart()) {
        outStatus = "Lightmap bake already running";
        return false;
    }

    DeleteFileIfExists(request.temporaryOutputPath);
    DeleteFileIfExists(MakeSectorObjectProbeSidecarPathForLightmapPath(request.temporaryOutputPath));
    DeleteFileIfExists(MakeSectorStaticModelSidecarPathForLightmapPath(request.temporaryOutputPath));

    state_.progress.phase.store(SectorLightmapBakePhase::Preparing);
    state_.progress.completedWork.store(0);
    state_.progress.totalWork.store(1);
    state_.progress.cancelRequested.store(false);
    state_.progress.running.store(true);
    state_.modalOpen = true;
    state_.awaitingAcknowledgement = false;
    state_.cancelButtonPressed = false;
    state_.terminalMessage.clear();
    state_.terminalSuccess = false;
    state_.terminalCancelled = false;
    state_.temporaryOutputPath = request.temporaryOutputPath;
    state_.startTimeSeconds = GetTime();
    state_.completedTimeSeconds = 0.0;
    {
        std::lock_guard<std::mutex> lock(state_.resultMutex);
        state_.pendingResult.reset();
    }

    LightmapBakeProgress* progress = &state_.progress;
    std::mutex* resultMutex = &state_.resultMutex;
    std::optional<SectorLightmapBakeAsyncResult>* pendingResult = &state_.pendingResult;
    state_.worker = std::thread([request = std::move(request), progress, resultMutex, pendingResult]() mutable {
        SectorTopologyLightmapBakeInput input;
        input.mapSnapshot = std::move(request.mapSnapshot);
        input.staticModels = std::move(request.staticModels);
        input.expectedSourceHash = std::move(request.expectedSourceHash);
        input.finalOutputPath = std::move(request.finalOutputPath);
        input.temporaryOutputPath = std::move(request.temporaryOutputPath);
        input.editorMapRevision = request.sourceMapRevision;

        SectorLightmapBakeAsyncResult asyncResult;
        asyncResult.expectedSourceHash = input.expectedSourceHash;
        asyncResult.sourceMapRevision = input.editorMapRevision;
        asyncResult.finalOutputPath = input.finalOutputPath;
        asyncResult.temporaryOutputPath = input.temporaryOutputPath;

        SectorLightmapBakeCallbacks callbacks;
        callbacks.onProgress = [progress](SectorLightmapBakePhase phase, uint32_t completedWork, uint32_t totalWork) {
            progress->phase.store(phase);
            progress->completedWork.store(completedWork);
            progress->totalWork.store(totalWork);
        };
        callbacks.isCancellationRequested = [progress]() {
            return progress->cancelRequested.load();
        };

        std::string error;
        const bool succeeded = BakeSectorLightmap(input, callbacks, asyncResult.bakeResult, error);
        asyncResult.cancelled = !succeeded && progress->cancelRequested.load();
        asyncResult.succeeded = succeeded && !asyncResult.cancelled;
        asyncResult.errorMessage = error.empty()
                ? (asyncResult.cancelled ? "Bake cancelled" : "Bake failed")
                : error;
        if (asyncResult.succeeded) {
            asyncResult.bakeReportText = FormatSectorLightmapBakeReport(asyncResult.bakeResult);
        }

        {
            std::lock_guard<std::mutex> lock(*resultMutex);
            *pendingResult = std::move(asyncResult);
        }

        if (progress->cancelRequested.load()) {
            progress->phase.store(SectorLightmapBakePhase::Cancelled);
        } else if (succeeded) {
            progress->phase.store(SectorLightmapBakePhase::Completed);
        } else {
            progress->phase.store(SectorLightmapBakePhase::Failed);
        }
        progress->completedWork.store(1);
        progress->totalWork.store(1);
        progress->running.store(false);
    });

    outStatus = "Baking lightmap...";
    return true;
}

SectorEditorLightmapBakePollResult SectorEditorLightmapBakeController::Poll()
{
    SectorEditorLightmapBakePollResult result;
    std::optional<SectorLightmapBakeAsyncResult> pending;
    {
        std::lock_guard<std::mutex> lock(state_.resultMutex);
        if (state_.pendingResult.has_value()) {
            pending = std::move(state_.pendingResult);
            state_.pendingResult.reset();
        }
    }

    if (pending.has_value()) {
        JoinWorker();
        state_.completedTimeSeconds = GetTime();
    }
    if (!pending.has_value()) {
        return result;
    }

    if (pending->cancelled) {
        state_.progress.phase.store(SectorLightmapBakePhase::Cancelled);
        DeleteFileIfExists(pending->temporaryOutputPath);
        DeleteFileIfExists(pending->bakeResult.objectProbes.path);
        DeleteFileIfExists(pending->bakeResult.staticModels.path);

        result.status = SectorEditorLightmapBakePollStatus::Cancelled;
        result.message = "Lightmap bake cancelled";
        state_.terminalMessage = result.message;
        state_.terminalSuccess = false;
        state_.terminalCancelled = true;
        state_.awaitingAcknowledgement = true;
        return result;
    }

    if (!pending->succeeded) {
        state_.progress.phase.store(SectorLightmapBakePhase::Failed);
        DeleteFileIfExists(pending->temporaryOutputPath);
        DeleteFileIfExists(pending->bakeResult.objectProbes.path);
        DeleteFileIfExists(pending->bakeResult.staticModels.path);

        result.status = SectorEditorLightmapBakePollStatus::Failed;
        result.message = pending->errorMessage.empty() ? "Bake failed" : pending->errorMessage;
        state_.terminalMessage = result.message;
        state_.terminalSuccess = false;
        state_.terminalCancelled = false;
        state_.awaitingAcknowledgement = true;
        TraceLog(LOG_WARNING, "%s", result.message.c_str());
        return result;
    }

    state_.progress.phase.store(SectorLightmapBakePhase::InstallingResult);
    result.status = SectorEditorLightmapBakePollStatus::Completed;
    result.completedResult = std::move(pending);
    return result;
}

bool SectorEditorLightmapBakeController::IsCompletedResultStale(
        const SectorLightmapBakeAsyncResult& result,
        const std::string& currentSourceHash) const
{
    return currentSourceHash != result.expectedSourceHash;
}

bool SectorEditorLightmapBakeController::InstallCompletedResultFiles(
        const SectorLightmapBakeAsyncResult& result,
        const std::string& currentSourceHash,
        SectorEditorLightmapBakeInstallPayload& outPayload) const
{
    outPayload = {};
    const std::string temporaryObjectProbePath = result.bakeResult.objectProbes.path.empty()
            ? MakeSectorObjectProbeSidecarPathForLightmapPath(result.temporaryOutputPath)
            : result.bakeResult.objectProbes.path;
    const SectorBakedStaticModelLightmapMetadata& staticMetadata =
            result.bakeResult.staticModels;
    const bool hasStaticModelMetadata =
            !staticMetadata.path.empty()
            || staticMetadata.version != 0
            || !staticMetadata.sourceHash.empty()
            || staticMetadata.modelCount != 0
            || staticMetadata.objectCount != 0
            || !staticMetadata.format.empty();
    const std::string temporaryStaticModelPath =
            !hasStaticModelMetadata
            ? std::string{}
            : (staticMetadata.path.empty()
                    ? MakeSectorStaticModelSidecarPathForLightmapPath(
                            result.temporaryOutputPath)
                    : staticMetadata.path);
    const auto cleanupTemps = [&]() {
        DeleteFileIfExists(result.temporaryOutputPath);
        DeleteFileIfExists(temporaryObjectProbePath);
        DeleteFileIfExists(temporaryStaticModelPath);
    };

    if (IsCompletedResultStale(result, currentSourceHash)) {
        cleanupTemps();
        outPayload.status = "Bake discarded: document changed during bake";
        return false;
    }
    if (hasStaticModelMetadata
            && (staticMetadata.path.empty()
                    || staticMetadata.version <= 0
                    || staticMetadata.sourceHash.empty()
                    || staticMetadata.modelCount <= 0
                    || staticMetadata.objectCount <= 0
                    || staticMetadata.format.empty())) {
        cleanupTemps();
        outPayload.status =
                "Bake failed: incomplete static model lightmap metadata";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(result.temporaryOutputPath, ec) || ec) {
        cleanupTemps();
        outPayload.status = "Bake failed: temporary lightmap output missing";
        return false;
    }
    if (!std::filesystem::exists(temporaryObjectProbePath, ec) || ec) {
        cleanupTemps();
        outPayload.status = "Bake failed: temporary object probe output missing";
        return false;
    }
    if (!temporaryStaticModelPath.empty()
            && (!std::filesystem::exists(temporaryStaticModelPath, ec) || ec)) {
        cleanupTemps();
        outPayload.status =
                "Bake failed: temporary static model lightmap output missing";
        return false;
    }

    std::vector<SectorBakedObjectLightProbe> validatedProbes;
    SectorBakedObjectLightProbeMetadata validatedProbeMetadata;
    std::string validationError;
    if (!ReadSectorBakedObjectLightProbeSidecar(
                temporaryObjectProbePath,
                &result.bakeResult.objectProbes,
                validatedProbes,
                validatedProbeMetadata,
                validationError)) {
        cleanupTemps();
        outPayload.status = "Bake failed: invalid object probe sidecar: "
                + validationError;
        return false;
    }
    if (!temporaryStaticModelPath.empty()) {
        SectorStaticModelLightmapData validatedStaticModels;
        if (!ReadSectorStaticModelLightmapSidecar(
                    temporaryStaticModelPath,
                    &result.bakeResult.staticModels,
                    validatedStaticModels,
                    validationError)) {
            cleanupTemps();
            outPayload.status =
                    "Bake failed: invalid static model lightmap sidecar: "
                    + validationError;
            return false;
        }
    }

    const std::filesystem::path finalPath(result.finalOutputPath);
    if (!finalPath.parent_path().empty()) {
        std::filesystem::create_directories(finalPath.parent_path(), ec);
        if (ec) {
            cleanupTemps();
            outPayload.status = TextFormat(
                    "Bake failed: could not create output directory: %s",
                    ec.message().c_str());
            return false;
        }
    }

    const std::string finalObjectProbePath = MakeSectorObjectProbeSidecarPathForLightmapPath(result.finalOutputPath);
    const std::string finalStaticModelPath =
            temporaryStaticModelPath.empty()
            ? std::string{}
            : MakeSectorStaticModelSidecarPathForLightmapPath(
                    result.finalOutputPath);
    if (!temporaryStaticModelPath.empty()) {
        std::filesystem::copy_file(
                temporaryStaticModelPath,
                finalStaticModelPath,
                std::filesystem::copy_options::overwrite_existing,
                ec);
        if (ec) {
            cleanupTemps();
            outPayload.status =
                    TextFormat(
                            "Bake failed: could not install static model lightmap sidecar: %s",
                            ec.message().c_str());
            return false;
        }
    }
    std::filesystem::copy_file(
            temporaryObjectProbePath,
            finalObjectProbePath,
            std::filesystem::copy_options::overwrite_existing,
            ec
    );
    if (ec) {
        cleanupTemps();
        DeleteFileIfExists(finalStaticModelPath);
        outPayload.status = TextFormat(
                "Bake failed: could not install object probe sidecar: %s",
                ec.message().c_str());
        return false;
    }
    std::filesystem::copy_file(
            result.temporaryOutputPath,
            result.finalOutputPath,
            std::filesystem::copy_options::overwrite_existing,
            ec
    );
    if (ec) {
        cleanupTemps();
        DeleteFileIfExists(finalObjectProbePath);
        DeleteFileIfExists(finalStaticModelPath);
        outPayload.status = TextFormat("Bake failed: could not install lightmap: %s", ec.message().c_str());
        return false;
    }
    cleanupTemps();

    outPayload.bakeResult = result.bakeResult;
    outPayload.finalLightmapPath = result.finalOutputPath;
    outPayload.finalObjectProbePath = finalObjectProbePath;
    outPayload.finalObjectProbeAssetPath = MakeSectorAssetRelativePath(finalObjectProbePath);
    outPayload.bakeResult.objectProbes.path = outPayload.finalObjectProbeAssetPath;
    outPayload.bakeResult.objectProbes.sourceHash = outPayload.bakeResult.sourceHash;
    if (!finalStaticModelPath.empty()) {
        outPayload.finalStaticModelPath = finalStaticModelPath;
        outPayload.finalStaticModelAssetPath =
                MakeSectorAssetRelativePath(finalStaticModelPath);
        outPayload.bakeResult.staticModels.path =
                outPayload.finalStaticModelAssetPath;
        outPayload.bakeResult.staticModels.sourceHash =
                outPayload.bakeResult.sourceHash;
    }
    return true;
}

bool SectorEditorLightmapBakeController::RequestCancel()
{
    if (!state_.progress.running.load()) {
        return false;
    }
    state_.progress.cancelRequested.store(true);
    state_.cancelButtonPressed = true;
    return true;
}

void SectorEditorLightmapBakeController::JoinWorker()
{
    if (state_.worker.joinable()) {
        state_.worker.join();
    }
}

void SectorEditorLightmapBakeController::Shutdown()
{
    if (state_.progress.running.load()) {
        state_.progress.cancelRequested.store(true);
    }
    JoinWorker();
    DeleteFileIfExists(state_.temporaryOutputPath);
    DeleteFileIfExists(MakeSectorObjectProbeSidecarPathForLightmapPath(state_.temporaryOutputPath));
    DeleteFileIfExists(MakeSectorStaticModelSidecarPathForLightmapPath(state_.temporaryOutputPath));
    state_.temporaryOutputPath.clear();
    {
        std::lock_guard<std::mutex> lock(state_.resultMutex);
        if (state_.pendingResult.has_value()) {
            DeleteFileIfExists(state_.pendingResult->temporaryOutputPath);
            DeleteFileIfExists(MakeSectorObjectProbeSidecarPathForLightmapPath(
                    state_.pendingResult->temporaryOutputPath));
            DeleteFileIfExists(MakeSectorStaticModelSidecarPathForLightmapPath(
                    state_.pendingResult->temporaryOutputPath));
            state_.pendingResult.reset();
        }
    }
    state_.modalOpen = false;
    state_.awaitingAcknowledgement = false;
    state_.cancelButtonPressed = false;
    state_.terminalMessage.clear();
    state_.terminalSuccess = false;
    state_.terminalCancelled = false;
    state_.completedTimeSeconds = 0.0;
    state_.progress.running.store(false);
    state_.progress.cancelRequested.store(false);
    state_.progress.completedWork.store(0);
    state_.progress.totalWork.store(0);
    state_.progress.phase.store(SectorLightmapBakePhase::Idle);
}

void SectorEditorLightmapBakeController::AcknowledgeTerminalState()
{
    state_.modalOpen = false;
    state_.awaitingAcknowledgement = false;
    state_.cancelButtonPressed = false;
    state_.terminalMessage.clear();
    state_.temporaryOutputPath.clear();
    state_.progress.phase.store(SectorLightmapBakePhase::Idle);
}

void SectorEditorLightmapBakeController::CompleteInstall(bool installed)
{
    state_.modalOpen = false;
    state_.awaitingAcknowledgement = false;
    state_.cancelButtonPressed = false;
    state_.terminalSuccess = installed;
    state_.terminalCancelled = false;
    state_.temporaryOutputPath.clear();
    state_.progress.phase.store(installed ? SectorLightmapBakePhase::Completed : SectorLightmapBakePhase::Failed);
}

SectorEditorLightmapBakeModalView SectorEditorLightmapBakeController::BuildModalView() const
{
    SectorEditorLightmapBakeModalView view;
    view.blocking = IsBlocking();
    view.running = state_.progress.running.load();
    view.awaitingAcknowledgement = state_.awaitingAcknowledgement;
    view.cancelButtonPressed = state_.cancelButtonPressed;
    view.terminalCancelled = state_.terminalCancelled;
    view.phase = state_.progress.phase.load();
    view.completedWork = state_.progress.completedWork.load();
    view.totalWork = state_.progress.totalWork.load();
    view.startTimeSeconds = state_.startTimeSeconds;
    view.completedTimeSeconds = state_.completedTimeSeconds;
    view.terminalMessage = state_.terminalMessage;
    return view;
}

} // namespace game
