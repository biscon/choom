#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorRuntimeReflectionProbes.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"
#include "sector_demo/renderer/SectorReflectionProbePolicy.h"
#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include <external/glad.h>
#include <rlgl.h>
#include <raymath.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace game
{
bool SectorRuntimeReflectionProbes::Initialize(
    engine::AssetManager &assets, engine::AssetScopeHandle scope, SectorPbrEnvironment &environment,
    std::size_t lightCapacity, std::size_t receiverCapacity, std::size_t objectCapacity)
{
    Shutdown();
    demand.collecting.assign(environment.localProbes.size(), 0);
    demand.requested.assign(environment.localProbes.size(), 0);
    environment.demandCollector = &demand;
    stats.demandedProbeIds.reserve(environment.localProbes.size());
    stats.demandedDirtyProbeIds.reserve(environment.localProbes.size());
    stats.deferredDirtyProbeIds.reserve(environment.localProbes.size());
    observed.reserve(lightCapacity + 2);
    current.reserve(lightCapacity + 2);
    snapshot.reserve(lightCapacity + 2);
    doors.reserve(objectCapacity);
    environment.blockers.reserve(environment.portals.size());
    draw.visibility.visibleSectorIds.reserve(receiverCapacity);
    draw.connectedVisibility.visibleSectorIds.reserve(receiverCapacity);
    draw.connectedVisibility.startSectorIds.reserve(1);
    draw.connectedVisibility.boundarySurfaceSectorIds.reserve(environment.portals.size());
    captureLights.ReserveReceiverBoundsCapacity(receiverCapacity, objectCapacity);
    captureLights.ReserveCaptureCapacity(lightCapacity + 2, receiverCapacity);
    captureLights.SetShadowMapResolution(256);
    captureLights.SetShadowFaceBudget(1);
    captureLights.SetSelectionFadeInSeconds(0);
    if (environment.localProbes.empty())
    {
        initialized = true;
        return true;
    }
    for (int i = 0; i < 3; ++i)
        raw[i] = assets.CreateRenderCubemap(scope, "reflection-capture-scratch", 64 << i);
    filterShader = LoadGameShader(GameShader::ReflectionFilter);
    bool ready = !engine::IsNull(raw[0]) && !engine::IsNull(raw[1]) && !engine::IsNull(raw[2]) &&
                 filterShader.id != 0 && filterShader.id != rlGetShaderIdDefault();
    for (int i = 0; i < 3 && ready; ++i)
    {
        auto &target = targets[i];
        engine::RenderTargetDescriptor d;
        d.debugName = "runtime-reflection-face";
        d.width = d.height = 64 << i;
        d.colorFormat = engine::RenderTargetColorFormat::Rgba16Float;
        ready = engine::LoadRenderTarget(d, target);
    }
    ready = ready && captureLights.LoadShadowMaterial() && captureLights.EnsureShadowMapResources();
    if (!ready)
    {
        for (auto &p : environment.localProbes)
            p.resourceFailed = p.failed = true;
        TraceLog(LOG_WARNING,
                 "Runtime reflections unavailable: GPU resource initialization failed");
        Shutdown();
        stats.failed = environment.localProbes.size();
        return false;
    }
    glGenFramebuffers(1, &framebuffer);
    glGenVertexArrays(1, &vertexArray);
    if (glQueryCounter)
        glGenQueries(static_cast<GLsizei>(queries.size()), queries.data());
    faceLoc = GetShaderLocation(filterShader, "faceIndex");
    sizeLoc = GetShaderLocation(filterShader, "faceSize");
    roughnessLoc = GetShaderLocation(filterShader, "roughness");
    copyLoc = GetShaderLocation(filterShader, "copyFace");
    int unit = 0;
    SetShaderValue(filterShader, GetShaderLocation(filterShader, "sourceCube"), &unit,
                   SHADER_UNIFORM_INT);
    unit = 1;
    SetShaderValue(filterShader, GetShaderLocation(filterShader, "sourceFace"), &unit,
                   SHADER_UNIFORM_INT);
    for (auto &p : environment.localProbes)
    {
        p.resourceFailed = !assets.GetCubemap(p.cubemap) || !assets.GetCubemap(p.inactive);
        MarkSectorReflectionProbeDirty(p, environment.seconds, true);
        for (int size = p.definition.resolution; size; size /= 2)
            stats.allocationBytes += static_cast<std::uint64_t>(size) * size * 6 * 8 * 2;
    }
    stats.allocationBytes += 2048ull * 2048 * 4; // 256px faces, 8x8 depth atlas (estimated).
    for (int resolution : {64, 128, 256})
        for (int size = resolution; size; size /= 2)
            stats.allocationBytes += static_cast<std::uint64_t>(size) * size * 6 * 8;
    for (const auto &t : targets)
        stats.allocationBytes += t.actual.estimatedAllocationBytes;
    initialized = true;
    return true;
}

void SectorRuntimeReflectionProbes::Shutdown()
{
    for (auto &target : targets)
        engine::UnloadRenderTarget(target);
    captureLights.UnloadShadowMapResources();
    captureLights.UnloadShadowMaterial();
    captureLights.Reset();
    if (filterShader.id && filterShader.id != rlGetShaderIdDefault())
        UnloadShader(filterShader);
    if (framebuffer)
        glDeleteFramebuffers(1, &framebuffer);
    if (vertexArray)
        glDeleteVertexArrays(1, &vertexArray);
    if (queries[0])
        glDeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
    filterShader = {};
    framebuffer = vertexArray = 0;
    queries.fill(0);
    queryPending.fill(false);
    active = -1;
    initialized = false;
    initialRequired = false;
    paused = false;
    stats = {};
    observed.clear();
    current.clear();
    snapshot.clear();
    doors.clear();
    querySlot = 0;
    timedTiles.fill(0);
    estimatedTileMs = 0.1;
}

void SectorRuntimeReflectionProbes::SnapshotDoors(engine::World *world)
{
    doors.clear();
    if (!world)
        return;
    world->ForEach<SectorObjectTransform, SectorDoor, SectorDoorRender>(
        [&](engine::Entity entity, SectorObjectTransform &transform, SectorDoor &,
            SectorDoorRender &render)
        {
            DoorPose pose;
            pose.entity = entity;
            pose.transform = transform;
            pose.widthAxis = render.widthAxis;
            pose.thicknessAxis = render.thicknessAxis;
            pose.model = world->Has<SectorDoorModelRender>(entity);
            if (pose.model)
            {
                const auto &model = world->Get<SectorDoorModelRender>(entity);
                pose.leaf = model.leafMatrix;
                pose.frame = model.frameMatrix;
                pose.receiverBounds = model.receiverBounds;
                pose.analyticBounds = model.analyticReceiverBounds;
            }
            if (doors.size() == doors.capacity())
                TraceLog(LOG_WARNING, "Reflection door snapshot exceeded reserved capacity");
            doors.push_back(pose);
        });
}

void SectorRuntimeReflectionProbes::SwapDoorPoses(engine::World *world)
{
    if (!world)
        return;
    // Render-only, non-structural substitution. Called in pairs around capture
    // work; the live simulation pose is restored before returning to the caller.
    for (auto &pose : doors)
    {
        if (!world->IsAlive(pose.entity) || !world->Has<SectorObjectTransform>(pose.entity) ||
            !world->Has<SectorDoorRender>(pose.entity))
            continue;
        std::swap(pose.transform, world->Get<SectorObjectTransform>(pose.entity));
        auto &render = world->Get<SectorDoorRender>(pose.entity);
        std::swap(pose.widthAxis, render.widthAxis);
        std::swap(pose.thicknessAxis, render.thicknessAxis);
        if (pose.model && world->Has<SectorDoorModelRender>(pose.entity))
        {
            auto &model = world->Get<SectorDoorModelRender>(pose.entity);
            std::swap(pose.leaf, model.leafMatrix);
            std::swap(pose.frame, model.frameMatrix);
            std::swap(pose.receiverBounds, model.receiverBounds);
            std::swap(pose.analyticBounds, model.analyticReceiverBounds);
        }
    }
}

void SectorRuntimeReflectionProbes::Invalidate(SectorPbrEnvironment &e, bool discontinuity)
{
    for (auto &p : e.localProbes)
    {
        MarkSectorReflectionProbeDirty(p, e.seconds, discontinuity);
    }
}

void SectorRuntimeReflectionProbes::ObserveLights(SectorPbrEnvironment &e,
                                                  const SectorDynamicLightingRenderer &lights,
                                                  float /*seconds*/)
{
    const std::size_t needed = lights.Sources().size() + 2;
    if (needed > current.capacity())
    {
        TraceLog(LOG_WARNING, "Runtime reflection light storage exceeded reserved capacity (%zu)",
                 needed);
        current.reserve(needed);
        observed.reserve(needed);
        snapshot.reserve(needed);
        captureLights.ReserveCaptureCapacity(needed, draw.visibility.visibleSectorIds.capacity());
    }
    current = lights.Sources();
    if (const auto *p = lights.RuntimePointLight())
        current.push_back(*p);
    if (const auto *p = lights.ReservedRuntimeLight())
        current.push_back(*p);
    for (auto &p : current)
    {
        p.light = NormalizeSectorReflectionLight(p.light);
    }
    const auto changed = [&](const auto *old, const auto *now)
    {
        if (old && now && SectorReflectionLightsMatch(old->light, now->light))
            return;
        const bool discrete = SectorReflectionLightDiscontinuity(old ? &old->light : nullptr,
                                                                 now ? &now->light : nullptr);
        for (auto &p : e.localProbes)
        {
            if ((old && SectorReflectionLightAffectsProbe(old->light, p.definition)) ||
                (now && SectorReflectionLightAffectsProbe(now->light, p.definition)))
            {
                MarkSectorReflectionProbeDirty(p, e.seconds, discrete);
            }
        }
    };
    for (const auto &now : current)
    {
        const auto it = std::find_if(
            observed.begin(), observed.end(), [&](const auto &old)
            { return old.light.kind == now.light.kind && old.lightId == now.lightId; });
        changed(it == observed.end() ? nullptr : &*it, &now);
    }
    for (const auto &old : observed)
    {
        const auto it = std::find_if(
            current.begin(), current.end(), [&](const auto &now)
            { return old.light.kind == now.light.kind && old.lightId == now.lightId; });
        if (it == current.end())
            changed(&old, static_cast<const SectorPreviewDynamicPointLightSource *>(nullptr));
    }
    observed = current;
}

void SectorRuntimeReflectionProbes::RequireInitial(SectorPbrEnvironment &e,
                                                   const RuntimePortalVisibilityResult &visibility,
                                                   int sector)
{
    if (initialRequired)
        return;
    initialRequired = true;
    for (auto &p : e.localProbes)
    {
        p.required =
            p.definition.topologySectorId == sector ||
            ShouldDrawRuntimeSectorForVisibility(p.definition.topologySectorId, visibility);
        for (const auto &edge : e.portals)
            if (edge.fromSectorId == sector && edge.toSectorId == p.definition.topologySectorId &&
                edge.open &&
                !std::any_of(e.blockers.begin(), e.blockers.end(), [&](const auto &b)
                             { return b.lineDefId == edge.lineDefId && b.blocksPortal; }))
                p.required = true;
    }
}
bool SectorRuntimeReflectionProbes::InitialReady(const SectorPbrEnvironment &e) const
{
    if (!initialized)
        return true;
    if (!initialRequired)
        return false;
    for (const auto &p : e.localProbes)
        if (!IsSectorReflectionProbePrepared(p))
            return false;
    return true;
}

bool SectorRuntimeReflectionProbes::CheckGlErrors(SectorReflectionFailureStage stage)
{
    DrainSectorReflectionGlErrors(failure, stage, [] { return glGetError(); });
    return failure.reason == SectorReflectionFailureReason::None;
}

bool SectorRuntimeReflectionProbes::FailCapture(SectorReflectionFailureStage stage,
        SectorReflectionFailureReason reason, int outputFace, int outputMip, int x, int y)
{
    if (failure.reason == SectorReflectionFailureReason::None) {
        failure.stage = stage;
        failure.reason = reason;
        failure.face = outputFace;
        failure.mip = outputMip;
        failure.tileX = x;
        failure.tileY = y;
    }
    return false;
}

bool SectorRuntimeReflectionProbes::FilterTile(engine::AssetManager &assets, unsigned int output,
                                               int resolution, int outputFace, int outputMip, int x,
                                               int y, bool copy)
{
    const auto stage = copy ? SectorReflectionFailureStage::Copy : SectorReflectionFailureStage::Filter;
    failure.face = outputFace;
    failure.mip = outputMip;
    failure.tileX = x;
    failure.tileY = y;
    const auto *cube = assets.GetCubemap(raw[resolution == 64 ? 0 : resolution == 128 ? 1 : 2]);
    if (!output || (!copy && (!cube || !cube->id)))
        return FailCapture(stage, SectorReflectionFailureReason::MissingCubemap,
                           outputFace, outputMip, x, y);
    const int size = std::max(1, resolution >> outputMip);
    const float roughness = outputMip / std::log2(static_cast<float>(resolution));
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + outputFace, output, outputMip);
    const unsigned int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        failure.framebufferStatus = status;
        FailCapture(stage, SectorReflectionFailureReason::IncompleteFramebuffer,
                    outputFace, outputMip, x, y);
    }
    if (!CheckGlErrors(stage)) return false;
    glViewport(0, 0, size, size);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, copy ? size : std::min(64, size - x), copy ? size : std::min(64, size - y));
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, !copy && cube ? cube->id : 0);
    glUseProgram(filterShader.id);
    glUniform1i(faceLoc, outputFace);
    glUniform1i(sizeLoc, size);
    glUniform1f(roughnessLoc, roughness);
    glUniform1i(copyLoc, copy ? 1 : 0);
    glBindVertexArray(vertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_SCISSOR_TEST);
    return CheckGlErrors(stage);
}

void SectorRuntimeReflectionProbes::BeginMainViewFrame()
{
    AdvanceSectorReflectionDemandFrame(demand);
}

void SectorRuntimeReflectionProbes::Step(engine::AssetManager &assets, SectorPbrEnvironment &e,
                                         SectorMeshRenderer &renderer, engine::World *world,
                                         SectorRuntimeDoorLightingContext doorLighting,
                                         bool preparing)
{
    stats.cpuMilliseconds = 0;
    stats.stageCpuMilliseconds.fill(0);
    stats.stage = SectorReflectionCaptureStage::Idle;
    stats.batchesDrawn = stats.batchesCulled = stats.objectsDrawn = stats.objectsCulled = 0;
    if (!initialized || e.localProbes.empty())
        return;
    const auto cpuStart = std::chrono::steady_clock::now();
    stats.ready = stats.failed = stats.queued = stats.required = stats.prepared = stats.retrying = 0;
    stats.demanded = stats.demandedDirty = stats.deferredDirty = 0;
    stats.demandedProbeIds.clear();
    stats.demandedDirtyProbeIds.clear();
    stats.deferredDirtyProbeIds.clear();
    const auto demanded = [&](std::size_t i) {
        return IsSectorReflectionProbeDemanded(e, demand, i, preparing);
    };
    for (std::size_t i = 0; i < e.localProbes.size(); ++i)
    {
        const auto &p = e.localProbes[i];
        stats.ready += p.ready;
        stats.failed += p.failed;
        stats.retrying += p.dirty && p.captureFailures > 0 && !p.failed;
        stats.queued += p.dirty && !p.failed;
        stats.required += p.required;
        stats.prepared += p.required && IsSectorReflectionProbePrepared(p);
        const int id = p.definition.sourceAuthoringProbeId;
        if (demanded(i)) {
            ++stats.demanded;
            stats.demandedProbeIds.push_back(id);
            if (p.dirty && !p.failed) {
                ++stats.demandedDirty;
                stats.demandedDirtyProbeIds.push_back(id);
            }
        } else if (p.dirty && !p.failed) {
            ++stats.deferredDirty;
            stats.deferredDirtyProbeIds.push_back(id);
        }
    }
    const int nextProbe = SelectSectorReflectionProbeUpdate(
            e, demand, preparing, renderer.Position(), active, paused);
    if (active >= 0 && nextProbe != active) {
        // The published cube remains untouched; a future request snapshots anew.
        active = -1;
        stats.activeProbeId = -1;
        ++stats.cancelled;
    }
    if (paused || nextProbe < 0)
        return;
    if (active >= 0 && e.localProbes[active].discontinuity != capturedDiscontinuity)
    {
        ++stats.discarded;
        active = -1;
        stats.activeProbeId = -1;
        return;
    }
    const bool starting = active < 0;
    if (starting) inheritedErrorsReported = false;
    failure = {};
    failure.probeId = e.localProbes[nextProbe].definition.sourceAuthoringProbeId;
    failure.attempt = e.localProbes[nextProbe].captureFailures + 1;
    // Flush queued main-view draws before establishing capture's GL error boundary.
    rlDrawRenderBatchActive();
    CheckGlErrors(SectorReflectionFailureStage::BeforeCapture);
    stats.inheritedGlErrors += failure.inheritedGlErrorCount;
    if (failure.inheritedGlErrorCount && !inheritedErrorsReported) {
        TraceLog(LOG_WARNING, "Reflection probe %d: pre-existing OpenGL error 0x%04x (%u errors); "
                 "not attributed to capture (attempt %d, further inherited errors counted in stats)",
                 failure.probeId, failure.inheritedGlError, failure.inheritedGlErrorCount,
                 failure.attempt);
        inheritedErrorsReported = true;
    }
    if (queryPending[querySlot])
    {
        GLint available = 0;
        glGetQueryObjectiv(queries[querySlot * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available)
        {
            GLuint64 begin = 0, end = 0;
            glGetQueryObjectui64v(queries[querySlot * 2], GL_QUERY_RESULT, &begin);
            glGetQueryObjectui64v(queries[querySlot * 2 + 1], GL_QUERY_RESULT, &end);
            stats.gpuMilliseconds = (end - begin) / 1e6;
            stats.lastStageGpuMilliseconds[static_cast<std::size_t>(timedStages[querySlot])] =
                    stats.gpuMilliseconds;
            stats.overruns += stats.gpuMilliseconds > 0.5;
            if (timedTiles[querySlot] > 0)
                estimatedTileMs = std::max(0.01, stats.gpuMilliseconds / timedTiles[querySlot]);
            queryPending[querySlot] = false;
        }
    }
    if (active < 0)
    {
        active = nextProbe;
        auto &p = e.localProbes[active];
        p.lastStarted = e.seconds;
        capturedAt = e.seconds;
        capturedRevision = p.revision;
        capturedDiscontinuity = p.discontinuity;
        face = 0;
        mip = 1;
        filterFace = tileX = tileY = 0;
        shadowsReady = false;
        shadowFrames = 0;
        snapshot = current;
        if (p.captureFailures > 0)
            captureLights.InvalidateShadowContents();
        captureLights.SetCaptureSources(snapshot);
        SnapshotDoors(world);
        draw.lighting = &captureLights;
        draw.seconds = 0;
        stats.activeProbeId = p.definition.sourceAuthoringProbeId;
    }
    auto &p = e.localProbes[active];
    GLint savedFbo = 0, savedReadFbo = 0, savedViewport[4], savedScissor[4], savedProgram = 0,
          savedVao = 0, savedActive = 0;
    GLint savedDepthFunc = 0, savedCullFace = 0, savedBlend[6];
    GLboolean savedDepthMask, savedColorMask[4];
    const int savedWidth = rlGetFramebufferWidth(), savedHeight = rlGetFramebufferHeight();
    const Matrix savedProjection = rlGetMatrixProjection(), savedModelview = rlGetMatrixModelview();
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFbo);
    glGetIntegerv(GL_VIEWPORT, savedViewport);
    glGetIntegerv(GL_SCISSOR_BOX, savedScissor);
    glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &savedVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActive);
    glGetIntegerv(GL_DEPTH_FUNC, &savedDepthFunc);
    glGetIntegerv(GL_CULL_FACE_MODE, &savedCullFace);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &savedDepthMask);
    glGetBooleanv(GL_COLOR_WRITEMASK, savedColorMask);
    const GLenum blendNames[] = {GL_BLEND_SRC_RGB,      GL_BLEND_DST_RGB,
                                 GL_BLEND_SRC_ALPHA,    GL_BLEND_DST_ALPHA,
                                 GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA};
    for (int i = 0; i < 6; ++i)
        glGetIntegerv(blendNames[i], &savedBlend[i]);
    GLint texture2d[16], textureCube[16];
    for (int i = 0; i < 16; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2d[i]);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &textureCube[i]);
    }
    const bool depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND),
               cull = glIsEnabled(GL_CULL_FACE), scissor = glIsEnabled(GL_SCISSOR_TEST);
    const bool seamless = glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    GLfloat clearColor[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    const bool timing = queries[0] && !queryPending[querySlot];
    if (timing)
        glQueryCounter(queries[querySlot * 2], GL_TIMESTAMP);
    bool success = CheckGlErrors(SectorReflectionFailureStage::Setup);
    int tiles = 0;
    draw.batchesDrawn = draw.batchesCulled = 0;
    draw.culling.objectsDrawn = draw.culling.objectsCulled = 0;
    SwapDoorPoses(world);
    if (starting && success) {
        renderer.PrepareReflectionCapture(draw, p.definition, world);
        success = CheckGlErrors(SectorReflectionFailureStage::Setup);
    }
    if (success && !shadowsReady)
    {
        stats.stage = SectorReflectionCaptureStage::Shadows;
        failure.previousPendingShadowFaces = captureLights.PendingShadowFaceCount();
        renderer.PrepareReflectionShadows(draw, world);
        failure.pendingShadowFaces = captureLights.PendingShadowFaceCount();
        failure.renderedShadowFaces = captureLights.ShadowRenderStats().renderedTiles;
        failure.shadowFrames = shadowFrames + 1;
        shadowsReady = failure.pendingShadowFaces == 0;
        success = CheckGlErrors(SectorReflectionFailureStage::Shadows);
        // Missing/failed alpha assets must not keep a load gate alive forever.
        if (++shadowFrames > static_cast<int>(MaxDynamicSpotLightShadowCasters) * 3 &&
            !shadowsReady)
            success = FailCapture(SectorReflectionFailureStage::Shadows,
                                  SectorReflectionFailureReason::ShadowTimeout);
    }
    else if (success && face < 6)
    {
        stats.stage = SectorReflectionCaptureStage::Scene;
        const int resolution = p.definition.resolution;
        auto &target = targets[resolution == 64 ? 0 : resolution == 128 ? 1 : 2];
        failure.face = face;
        failure.mip = 0;
        renderer.DrawReflectionFace(assets, draw, p.definition, face, target, world, doorLighting);
        success = CheckGlErrors(SectorReflectionFailureStage::Scene);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, target.native.texture.id);
        const auto *rawTexture = assets.GetCubemap(raw[resolution == 64    ? 0
                                                       : resolution == 128 ? 1
                                                                           : 2]);
        const auto *output = assets.GetCubemap(p.inactive);
        if (success && (!rawTexture || !output))
            success = FailCapture(SectorReflectionFailureStage::Copy,
                                  SectorReflectionFailureReason::MissingCubemap, face, 0);
        success = CheckGlErrors(SectorReflectionFailureStage::Copy) && success;
        success = success && FilterTile(assets, rawTexture->id, resolution, face, 0, 0, 0, true) &&
                  FilterTile(assets, output->id, resolution, face, 0, 0, 0, true);
        ++face;
    }
    else if (success)
    {
        stats.stage = SectorReflectionCaptureStage::Filter;
        const auto *output = assets.GetCubemap(p.inactive);
        if (!output)
            success = FailCapture(SectorReflectionFailureStage::Filter,
                    SectorReflectionFailureReason::MissingCubemap, filterFace, mip, tileX, tileY);
        const int count = std::clamp(static_cast<int>(0.5 / estimatedTileMs), 1, 6);
        for (int i = 0; i < count && mip < p.mipCount && success; ++i)
        {
            ++tiles;
            success = FilterTile(assets, output->id, p.definition.resolution, filterFace,
                                           mip, tileX, tileY, false);
            const int size = std::max(1, p.definition.resolution >> mip);
            tileX += 64;
            if (tileX >= size)
            {
                tileX = 0;
                tileY += 64;
            }
            if (tileY >= size)
            {
                tileY = 0;
                ++filterFace;
            }
            if (filterFace == 6)
            {
                filterFace = 0;
                ++mip;
            }
        }
    }
    SwapDoorPoses(world);
    if (timing)
    {
        glQueryCounter(queries[querySlot * 2 + 1], GL_TIMESTAMP);
        queryPending[querySlot] = true;
        timedTiles[querySlot] = tiles;
        timedStages[querySlot] = stats.stage;
    }
    querySlot = (querySlot + 1) % queryPending.size();
    success = CheckGlErrors(SectorReflectionFailureStage::Setup) && success;
    // Restore raylib's current-FBO dimensions as well as OpenGL: BeginMode3D
    // derives its aspect ratio from this state, not just the GL viewport.
    if (savedFbo)
    {
        RenderTexture2D previous{};
        previous.id = savedFbo;
        previous.texture.width = savedWidth;
        previous.texture.height = savedHeight;
        BeginTextureMode(previous);
    }
    else
        EndTextureMode();
    rlSetMatrixProjection(savedProjection);
    rlSetMatrixModelview(savedModelview);
    rlSetFramebufferWidth(savedWidth);
    rlSetFramebufferHeight(savedHeight);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, savedFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, savedReadFbo);
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
    glScissor(savedScissor[0], savedScissor[1], savedScissor[2], savedScissor[3]);
    glDepthMask(savedDepthMask);
    glColorMask(savedColorMask[0], savedColorMask[1], savedColorMask[2], savedColorMask[3]);
    glDepthFunc(savedDepthFunc);
    glCullFace(savedCullFace);
    glBlendFuncSeparate(savedBlend[0], savedBlend[1], savedBlend[2], savedBlend[3]);
    glBlendEquationSeparate(savedBlend[4], savedBlend[5]);
    for (int i = 0; i < 16; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, texture2d[i]);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureCube[i]);
    }
    glUseProgram(savedProgram);
    glBindVertexArray(savedVao);
    glActiveTexture(savedActive);
    if (depth)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (cull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (scissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    if (seamless)
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    else
        glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    success = CheckGlErrors(SectorReflectionFailureStage::Restore) && success;
    stats.face = face;
    stats.mip = mip;
    if (!success)
    {
        FailSectorReflectionProbeCapture(p, e.seconds);
        // A failed draw can leave a shadow tile marked valid despite bad contents.
        // Rebuild the capture atlas on the next job, without reallocating resources.
        captureLights.InvalidateShadowContents();
        stats.lastFailure = failure;
        active = -1;
        TraceLog(LOG_WARNING, "Reflection probe %d capture failed: stage=%s reason=%s "
                 "attempt=%d/%d face=%d mip=%d tile=%d,%d GL=0x%04x errors=%u FBO=0x%04x "
                 "shadowFrames=%d pending=%zu previousPending=%zu rendered=%zu; %s (delay=%.2fs)",
                 failure.probeId, SectorReflectionFailureStageName(failure.stage),
                 SectorReflectionFailureReasonName(failure.reason), failure.attempt,
                 SectorReflectionMaxCaptureAttempts, failure.face, failure.mip, failure.tileX,
                 failure.tileY, failure.glError, failure.glErrorCount, failure.framebufferStatus,
                 failure.shadowFrames, failure.pendingShadowFaces, failure.previousPendingShadowFaces,
                 failure.renderedShadowFaces, p.failed ? "retries exhausted" : "retry queued",
                 p.failed ? 0.0 : p.retryAt - e.seconds);
    }
    else if (mip >= p.mipCount)
    {
        PublishSectorReflectionProbe(p, e.seconds, capturedRevision);
        ++stats.completed;
        active = -1;
    }
    stats.cpuMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cpuStart)
            .count();
    stats.stageCpuMilliseconds[static_cast<std::size_t>(stats.stage)] = stats.cpuMilliseconds;
    stats.batchesDrawn = draw.batchesDrawn;
    stats.batchesCulled = draw.batchesCulled;
    stats.objectsDrawn = draw.culling.objectsDrawn;
    stats.objectsCulled = draw.culling.objectsCulled;
    if (active < 0) stats.activeProbeId = -1;
}
} // namespace game
