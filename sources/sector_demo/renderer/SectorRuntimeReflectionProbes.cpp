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
namespace
{
const char *FilterVs = R"(#version 330
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}
)";
const char *FilterFs = R"(#version 330
uniform samplerCube sourceCube;
uniform sampler2D sourceFace;
uniform int faceIndex;
uniform int faceSize;
uniform int copyFace;
uniform float roughness;
out vec4 finalColor;
vec3 direction(vec2 uv) {
    vec2 p=uv*2.0-1.0;
    if(faceIndex==0)return normalize(vec3(1,-p.y,-p.x));
    if(faceIndex==1)return normalize(vec3(-1,-p.y,p.x));
    if(faceIndex==2)return normalize(vec3(p.x,1,p.y));
    if(faceIndex==3)return normalize(vec3(p.x,-1,-p.y));
    if(faceIndex==4)return normalize(vec3(p.x,-p.y,1));
    return normalize(vec3(-p.x,-p.y,-1));
}
float radical(uint bits) {
    bits=(bits<<16u)|(bits>>16u);
    bits=((bits&0x55555555u)<<1u)|((bits&0xAAAAAAAAu)>>1u);
    bits=((bits&0x33333333u)<<2u)|((bits&0xCCCCCCCCu)>>2u);
    bits=((bits&0x0F0F0F0Fu)<<4u)|((bits&0xF0F0F0F0u)>>4u);
    bits=((bits&0x00FF00FFu)<<8u)|((bits&0xFF00FF00u)>>8u);
    return float(bits)*2.3283064365386963e-10;
}
void main() {
    vec2 uv=gl_FragCoord.xy/float(faceSize);
    // Match the existing engine camera basis to OpenGL cubemap coordinates.
    if(copyFace!=0){finalColor=vec4(texture(sourceFace,vec2(1)-uv).rgb,1);return;}
    vec3 n=direction(uv);
    vec3 up=abs(n.z)<0.999?vec3(0,0,1):vec3(1,0,0);
    vec3 tangent=normalize(cross(up,n)),bitangent=cross(n,tangent);
    float a=roughness*roughness;
    vec3 sum=vec3(0);float weight=0;
    for(uint i=0u;i<64u;++i){
        float phi=6.28318530718*float(i)/64.0;
        float y=radical(i);
        float c=sqrt((1.0-y)/(1.0+(a*a-1.0)*y));
        float s=sqrt(max(0.0,1.0-c*c));
        vec3 h=tangent*cos(phi)*s+bitangent*sin(phi)*s+n*c;
        vec3 l=normalize(2.0*dot(n,h)*h-n);
        float w=max(dot(n,l),0.0);
        sum+=textureLod(sourceCube,l,0.0).rgb*w;weight+=w;
    }
    finalColor=vec4(sum/max(weight,0.00001),1);
}
)";

} // namespace

bool SectorRuntimeReflectionProbes::Initialize(
    engine::AssetManager &assets, engine::AssetScopeHandle scope, SectorPbrEnvironment &environment,
    std::size_t lightCapacity, std::size_t receiverCapacity, std::size_t objectCapacity)
{
    Shutdown();
    observed.reserve(lightCapacity + 2);
    current.reserve(lightCapacity + 2);
    snapshot.reserve(lightCapacity + 2);
    doors.reserve(objectCapacity);
    environment.blockers.reserve(environment.portals.size());
    draw.visibility.visibleSectorIds.reserve(receiverCapacity);
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
    filterShader = LoadShaderFromMemory(FilterVs, FilterFs);
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
            p.failed = true;
        TraceLog(LOG_WARNING,
                 "Runtime reflections unavailable: GPU resource initialization failed");
        Shutdown();
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
    for (const auto &p : environment.localProbes)
    {
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
                                                  float seconds)
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
        p.light.intensity = DynamicLightEffectiveUploadIntensity(p.light, seconds);
        p.light.flicker = false;
        p.light.selectionFadeEnabled = false;
        p.light.selectionFadeMultiplier = 1;
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
        if (p.required && !p.ready && !p.failed)
            return false;
    return true;
}

bool SectorRuntimeReflectionProbes::FilterTile(engine::AssetManager &assets, unsigned int output,
                                               int resolution, int outputFace, int outputMip, int x,
                                               int y, bool copy)
{
    const int size = std::max(1, resolution >> outputMip);
    const float roughness = outputMip / std::log2(static_cast<float>(resolution));
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + outputFace, output, outputMip);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return false;
    glViewport(0, 0, size, size);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, copy ? size : std::min(64, size - x), copy ? size : std::min(64, size - y));
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    const auto *cube = assets.GetCubemap(raw[resolution == 64 ? 0 : resolution == 128 ? 1 : 2]);
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
    return glGetError() == GL_NO_ERROR;
}

void SectorRuntimeReflectionProbes::Step(engine::AssetManager &assets, SectorPbrEnvironment &e,
                                         SectorMeshRenderer &renderer, engine::World *world,
                                         SectorRuntimeDoorLightingContext doorLighting,
                                         bool preparing)
{
    if (!initialized || e.localProbes.empty())
        return;
    const auto cpuStart = std::chrono::steady_clock::now();
    stats.ready = stats.failed = stats.queued = stats.required = stats.prepared = 0;
    for (const auto &p : e.localProbes)
    {
        stats.ready += p.ready;
        stats.failed += p.failed;
        stats.queued += p.dirty && !p.failed;
        stats.required += p.required;
        stats.prepared += p.required && (p.ready || p.failed);
    }
    if (paused)
        return;
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
            stats.overruns += stats.gpuMilliseconds > 0.5;
            if (timedTiles[querySlot] > 0)
                estimatedTileMs = std::max(0.01, stats.gpuMilliseconds / timedTiles[querySlot]);
            queryPending[querySlot] = false;
        }
    }
    if (active < 0)
    {
        double score = -1e30;
        for (std::size_t i = 0; i < e.localProbes.size(); ++i)
        {
            const auto &p = e.localProbes[i];
            if (!CanStartSectorReflectionProbe(p, e.seconds, preparing))
                continue;
            const double priority =
                (preparing && p.required ? 10000 : 0) +
                (ShouldDrawRuntimeSectorForVisibility(p.definition.topologySectorId,
                                                      renderer.VisibilityResult())
                     ? 100
                     : 0) +
                (e.seconds - p.dirtySince) * 10 -
                Vector3Distance(renderer.Position(), p.definition.capturePositionWorld) * 0.01;
            if (priority > score)
            {
                score = priority;
                active = static_cast<int>(i);
            }
        }
        if (active < 0)
        {
            stats.activeProbeId = -1;
            return;
        }
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
        captureLights.SetCaptureSources(snapshot);
        SnapshotDoors(world);
        draw.lighting = &captureLights;
        draw.seconds = 0;
        renderer.PrepareReflectionCapture(draw, p.definition, world);
        stats.activeProbeId = p.definition.sourceAuthoringProbeId;
    }
    auto &p = e.localProbes[active];
    if (p.discontinuity != capturedDiscontinuity)
    {
        ++stats.discarded;
        active = -1;
        return;
    }
    rlDrawRenderBatchActive();
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
    bool success = true;
    int tiles = 0;
    SwapDoorPoses(world);
    if (!shadowsReady)
    {
        renderer.PrepareReflectionShadows(draw, world);
        shadowsReady = !captureLights.HasPendingShadowFaces();
        // Missing/failed alpha assets must not keep a load gate alive forever.
        if (++shadowFrames > static_cast<int>(MaxDynamicSpotLightShadowCasters) * 3 &&
            !shadowsReady)
            success = false;
    }
    else if (face < 6)
    {
        const int resolution = p.definition.resolution;
        auto &target = targets[resolution == 64 ? 0 : resolution == 128 ? 1 : 2];
        renderer.DrawReflectionFace(assets, draw, p.definition, face, target, world, doorLighting);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, target.native.texture.id);
        const auto *rawTexture = assets.GetCubemap(raw[resolution == 64    ? 0
                                                       : resolution == 128 ? 1
                                                                           : 2]);
        const auto *output = assets.GetCubemap(p.inactive);
        success = rawTexture && output &&
                  FilterTile(assets, rawTexture->id, resolution, face, 0, 0, 0, true) &&
                  FilterTile(assets, output->id, resolution, face, 0, 0, 0, true);
        ++face;
    }
    else
    {
        const auto *output = assets.GetCubemap(p.inactive);
        const int count = std::clamp(static_cast<int>(0.5 / estimatedTileMs), 1, 6);
        for (int i = 0; i < count && mip < p.mipCount && success; ++i)
        {
            ++tiles;
            success = output && FilterTile(assets, output->id, p.definition.resolution, filterFace,
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
    }
    querySlot = (querySlot + 1) % queryPending.size();
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
    stats.face = face;
    stats.mip = mip;
    if (!success)
    {
        p.failed = true;
        active = -1;
        TraceLog(LOG_WARNING, "Reflection probe %d capture failed",
                 p.definition.sourceAuthoringProbeId);
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
}
} // namespace game
