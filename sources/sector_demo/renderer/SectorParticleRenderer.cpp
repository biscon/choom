#include "sector_demo/renderer/SectorParticleRenderer.h"
#include "game/LoadShader.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLocalFogLighting.h"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace game {
namespace {
engine::ParticleHit TraceParticle(void* user, Vector3 from, Vector3 to)
{
    const auto* collision = static_cast<const SectorCollisionWorld*>(user);
    const Vector3 delta = Vector3Subtract(to, from);
    const float length = Vector3Length(delta);
    if (collision == nullptr || length <= 0.000001f) return {};
    const auto hit = collision->Raycast(from, Vector3Scale(delta, 1 / length), length);
    return {hit.hit, hit.position, hit.normal};
}
}
bool SectorParticleRenderer::Initialize(engine::AssetManager& assets, engine::AssetScopeHandle scope,
        const std::vector<SectorCompiledParticleEmitter>& emitters, size_t capacity)
{
    Shutdown();
    sources = emitters;
    if (emitters.empty()) return true;
    engine::InitializeParticlePool(pool, capacity, emitters.size());
    textures.resize(emitters.size()); drawItems.reserve(capacity); instances.resize(capacity);
    const auto request = [&](const std::string& path) {
        const auto resolved = ResolveSectorAssetPath(path);
        return assets.RequestTexture(scope, path.c_str(), resolved.c_str(), engine::TextureColorUsage::SceneSrgb,
                engine::TextureLoad_Mipmaps | engine::TextureLoad_TrilinearFilter);
    };
    // Pack supplied art once during this explicit load phase. All default layers
    // then share one texture, preserving global depth order in a single draw.
    // 16px cell gutters plus the generated transparent margins protect mipmaps.
    constexpr int Cell = 512, Padding = 16, Content = Cell - 2 * Padding;
    Image atlas = GenImageColor(Cell * 4, Cell * 2, BLANK);
    const char* paths[]{SectorParticleTexturePath(0),SectorParticleTexturePath(1),
            SectorParticleTexturePath(2),SectorParticleTexturePath(3),
            "assets/particles/flame_alt.png","assets/particles/smoke_alt.png","assets/particles/vapor_alt.png"};
    if (atlas.data) {
        for (int cell = 0; cell < 7; ++cell) {
            Image image = LoadImage(ResolveSectorAssetPath(paths[cell]).c_str());
            if (!image.data && cell >= 4) image = LoadImage(ResolveSectorAssetPath(paths[cell-4]).c_str());
            if (!image.data) continue;
            ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            ImageResize(&image,Content,Content);
            if (image.data) for (int y = 0; y < Content; ++y) {
                const size_t destination = static_cast<size_t>((cell/4*Cell+Padding+y)*atlas.width + cell%4*Cell+Padding)*4;
                std::memcpy(static_cast<unsigned char*>(atlas.data)+destination,
                        static_cast<unsigned char*>(image.data)+static_cast<size_t>(y)*Content*4,Content*4);
            }
            UnloadImage(image);
        }
        const auto handle = assets.CreateTextureFromImage(scope,"particle_builtin_atlas",atlas,
                engine::TextureColorUsage::SceneSrgb,engine::TextureLoad_Mipmaps | engine::TextureLoad_TrilinearFilter);
        defaults.fill(handle);
        UnloadImage(atlas);
    }
    for (size_t i = 0; i < emitters.size(); ++i) {
        auto& e = pool.emitters[i];
        e.definition = CompileSectorParticleDefinition(emitters[i]);
        e.enabled = emitters[i].enabled && emitters[i].sectorId > 0;
        e.intensity = emitters[i].settings.intensity * emitters[i].runtimeIntensity;
        for (int role = 0; role < 4; ++role) {
            const auto& path = emitters[i].settings.textures[role].path;
            textures[i][role] = path.empty() ? defaults[role] : request(path);
        }
    }
    Restart();
    shader = LoadGameShader(GameShader::Particle);
    if (shader.id == 0) return false;
    matrixLoc = GetShaderLocation(shader, "mvp"); textureLoc = GetShaderLocation(shader, "particleTexture");
    depthLoc = GetShaderLocation(shader, "sceneDepth"); hasDepthLoc = GetShaderLocation(shader, "hasDepth");
    viewportLoc = GetShaderLocation(shader, "viewportSize"); nearFarLoc = GetShaderLocation(shader, "nearFar");
    displayLoc = GetShaderLocation(shader, "displaySrgb");
    const float quad[]{-1,-1,0,1, 1,-1,1,1, 1,1,1,0, -1,-1,0,1, 1,1,1,0, -1,1,0,0};
    vao = rlLoadVertexArray(); rlEnableVertexArray(vao);
    quadVbo = rlLoadVertexBuffer(quad, sizeof(quad), false);
    rlSetVertexAttribute(0, 4, RL_FLOAT, false, 4 * sizeof(float), 0); rlEnableVertexAttribute(0);
    instanceVbo = rlLoadVertexBuffer(nullptr, static_cast<int>(capacity * sizeof(Instance)), true);
    for (int a = 1; a <= 5; ++a) {
        rlSetVertexAttribute(a, 4, RL_FLOAT, false, sizeof(Instance), (a - 1) * sizeof(Vector4));
        rlEnableVertexAttribute(a); rlSetVertexAttributeDivisor(a, 1);
    }
    rlDisableVertexBuffer(); rlDisableVertexArray();
    return vao && quadVbo && instanceVbo;
}
bool SectorParticleRenderer::Reconfigure(const std::vector<SectorCompiledParticleEmitter>& emitters)
{
    if (!shader.id || emitters.size() != sources.size()) return false;
    for (size_t i = 0; i < emitters.size(); ++i)
        for (int role = 0; role < 4; ++role)
            if (sources[i].settings.textures[role].path != emitters[i].settings.textures[role].path) return false;
    sources = emitters;
    for (size_t i = 0; i < emitters.size(); ++i) {
        pool.emitters[i].definition = CompileSectorParticleDefinition(emitters[i]);
        pool.emitters[i].enabled = emitters[i].enabled && emitters[i].sectorId > 0;
        pool.emitters[i].intensity = emitters[i].settings.intensity * emitters[i].runtimeIntensity;
    }
    Restart(); return true;
}
void SectorParticleRenderer::Shutdown()
{
    if (vao) rlUnloadVertexArray(vao);
    if (quadVbo) rlUnloadVertexBuffer(quadVbo);
    if (instanceVbo) rlUnloadVertexBuffer(instanceVbo);
    if (shader.id) UnloadShader(shader);
    vao = quadVbo = instanceVbo = 0; shader = {};
    pool = {}; sources.clear(); textures.clear(); drawItems.clear(); instances.clear();
    needsPrewarm = true;
}
void SectorParticleRenderer::Restart()
{
    engine::ResetParticlePool(pool); needsPrewarm = true;
}
void SectorParticleRenderer::Update(float dt, const std::vector<SectorCompiledParticleEmitter>& emitters,
        const SectorCollisionWorld* collision, const Camera3D& camera, bool prewarm, bool previewFloor)
{
    if (emitters.size() != pool.emitters.size()) return; // Rebuild only in an explicit load/edit phase.
    for (size_t i = 0; i < emitters.size(); ++i) {
        auto& target = pool.emitters[i];
        target.enabled = emitters[i].enabled && emitters[i].sectorId > 0;
        target.intensity = emitters[i].settings.intensity * emitters[i].runtimeIntensity;
        if (emitters[i].pendingBurst > 0) {
            engine::TriggerParticleBurst(pool, i, emitters[i].pendingBurst);
            emitters[i].pendingBurst = 0;
        }
    }
    engine::ParticleUpdateContext context;
    context.user = const_cast<SectorCollisionWorld*>(collision); context.trace = collision ? TraceParticle : nullptr;
    if (previewFloor && !collision) context.trace = [](void*,Vector3 from,Vector3 to) -> engine::ParticleHit {
        if (to.y >= 0 || from.y < 0) return {};
        const float t = from.y / std::max(0.000001f,from.y-to.y);
        return {true,Vector3Lerp(from,to,t),{0,1,0}};
    };
    context.camera = camera.position; context.distanceLod = !previewFloor;
    if (needsPrewarm) {
        if (prewarm) engine::PrewarmParticles(pool, context);
        needsPrewarm = false;
    }
    engine::UpdateParticles(pool, dt, context);
}
void SectorParticleRenderer::Draw(engine::AssetManager& assets, RenderTexture2D target, const Texture2D* depth,
        const Camera3D& camera, const SectorTopologyMap* map, const SectorCollisionWorld* collision,
        const SectorBakedObjectLightProbeRuntimeData* probes, const SectorBillboardDynamicLightContext* lights,
        const RuntimePortalVisibilityResult* visibility, bool displaySrgb)
{
    const auto start = std::chrono::steady_clock::now();
    pool.diagnostics.visible = pool.diagnostics.drawCalls = 0;
    pool.diagnostics.drawCpuMilliseconds = 0;
    if (!shader.id || !vao || pool.count == 0) return;
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3CrossProduct(right, forward);
    const float tanY = std::tan(camera.fovy * DEG2RAD * 0.5f);
    const float tanX = tanY * target.texture.width / std::max(1.0f, static_cast<float>(target.texture.height));
    drawItems.clear();
    for (size_t i = 0; i < pool.count; ++i) {
        auto& p = pool.particles[i];
        const auto& d = pool.emitters[p.emitter].definition;
        const auto& layer = d.layers[p.layer];
        const Vector3 relative = Vector3Subtract(p.position, camera.position);
        const float z = Vector3DotProduct(relative, forward);
        const float size = engine::ParticleSize(p, layer) * (layer.streak ? 5 : 1);
        if (z + size <= 0 || Vector3LengthSqr(relative) > (d.maxDistance + size) * (d.maxDistance + size)
                || std::fabs(Vector3DotProduct(relative, right)) > std::max(0.0f, z) * tanX + size
                || std::fabs(Vector3DotProduct(relative, up)) > std::max(0.0f, z) * tanY + size) continue;
        if (collision != nullptr) {
            p.sectorId = collision->FindSectorContainingPointPreferCurrent({p.position.x, p.position.z}, p.sectorId);
            if (visibility && !ShouldDrawRuntimeSectorForVisibility(p.sectorId, *visibility)) {
                // Billboards can overlap a visible neighboring sector even when
                // their center has crossed a portal.
                bool overlap = false;
                const BoundingBox bounds{Vector3Subtract(p.position, {size,size,size}),
                        Vector3Add(p.position, {size,size,size})};
                for (int sector : visibility->visibleSectorIds)
                    if (collision->BoundsOverlapSector(sector, bounds)) { overlap = true; break; }
                if (!overlap) continue;
            }
        }
        const int role = layer.textureRole;
        const auto* texture = assets.GetTexture(textures[p.emitter][role]);
        bool builtin = sources[p.emitter].settings.textures[role].path.empty();
        if (texture == nullptr || texture->id == 0) { texture = assets.GetTexture(defaults[role]); builtin = true; }
        if (texture == nullptr || texture->id == 0) continue;
        if (map && !probes && layer.emissive == 0) {
            const auto* sector = FindSectorTopologySector(*map,p.sectorId);
            if (sector) p.lighting = Vector3Scale(SectorLightmapAuthoredSrgbColorToLinear(sector->ambientColor),sector->ambientIntensity);
        }
        if (map && probes && p.lightingAge >= 0.2f && layer.emissive == 0) {
            p.lighting = EvaluateSectorLocalFogProbeLighting(SampleBakedObjectLighting(*probes, p.position, p.sectorId, map));
            p.lightingAge = 0;
        }
        drawItems.push_back({i, z, texture->id, role, builtin ? role + (role < 3 && p.phase >= PI ? 4 : 0) : -1});
    }
    std::sort(drawItems.begin(), drawItems.end(), [](const auto& a, const auto& b) {
        return a.depth != b.depth ? a.depth > b.depth : a.particle < b.particle;
    });
    for (size_t index = 0; index < drawItems.size(); ++index) {
        const auto& item = drawItems[index];
        const auto& p = pool.particles[item.particle];
        const auto& d = pool.emitters[p.emitter].definition;
        const auto& layer = d.layers[p.layer];
        Vector3 lighting = p.lighting;
        if (lights && layer.emissive == 0) {
            for (int l = 0; l < lights->dynamicLightCount; ++l) {
                Vector3 delta = Vector3Subtract(p.position, lights->dynamicLightPositions[l]);
                const float dist = Vector3Length(delta), radius = lights->dynamicLightRadii[l];
                if (radius <= 0 || dist >= radius) continue;
                float weight = (1 - dist / radius); weight *= weight;
                if (lights->dynamicLightTypes[l] == 1 && dist > 0.0001f) {
                    const float cosine = Vector3DotProduct(Vector3Scale(delta, 1 / dist), lights->dynamicLightDirections[l]);
                    weight *= std::clamp((cosine - lights->dynamicLightOuterConeCos[l]) /
                            std::max(0.001f, lights->dynamicLightInnerConeCos[l] - lights->dynamicLightOuterConeCos[l]), 0.0f, 1.0f);
                }
                lighting = Vector3Add(lighting, Vector3Scale(lights->dynamicLightColors[l], weight * lights->dynamicLightIntensities[l]));
            }
        }
        const Vector3 color = Vector3Scale(Vector3Multiply(layer.color,
                layer.emissive > 0 ? Vector3{1,1,1} : lighting), layer.emissive > 0 ? layer.emissive : 1);
        const float size = engine::ParticleSize(p, layer) * 0.5f;
        Vector3 particleRight = Vector3Add(Vector3Scale(right, std::cos(p.rotation)), Vector3Scale(up, std::sin(p.rotation)));
        Vector3 particleUp = Vector3Add(Vector3Scale(up, std::cos(p.rotation)), Vector3Scale(right, -std::sin(p.rotation)));
        float length = size;
        if (layer.streak) {
            const Vector3 projected = Vector3Subtract(p.velocity, Vector3Scale(forward, Vector3DotProduct(p.velocity, forward)));
            if (Vector3LengthSqr(projected) > 0.0001f) {
                particleUp = Vector3Normalize(projected);
                particleRight = Vector3Normalize(Vector3CrossProduct(particleUp, forward));
            }
            length *= std::clamp(Vector3Length(p.velocity), 1.0f, 5.0f);
        }
        particleRight = Vector3Scale(particleRight, size); particleUp = Vector3Scale(particleUp, length);
        const float distance = Vector3Distance(p.position, camera.position);
        const float fade = std::clamp((d.maxDistance - distance) / std::max(1.0f, d.maxDistance * 0.2f), 0.0f, 1.0f);
        const auto& texture = sources[p.emitter].settings.textures[item.role];
        const bool fallback = item.atlasCell >= 0;
        const int columns = fallback ? 1 : texture.columns, rows = fallback ? 1 : texture.rows;
        const int frame = fallback ? 0 : engine::ParticleFlipbookFrame(p.age, texture.fps, texture.frames,
                texture.randomStart ? p.phase / (2 * PI) : 0);
        Vector4 uv{static_cast<float>(frame % columns) / columns, static_cast<float>(frame / columns) / rows,
                1.0f / columns, 1.0f / rows};
        if (item.atlasCell >= 0) {
            uv = {(item.atlasCell % 4 * 512 + 16.0f) / 2048,
                    (item.atlasCell / 4 * 512 + 16.0f) / 1024,480.0f/2048,480.0f/1024};
        }
        instances[index] = {{p.position.x, p.position.y, p.position.z, size},
                {particleRight.x, particleRight.y, particleRight.z, layer.emissive > 0 ? 1.0f : 0.0f},
                {particleUp.x, particleUp.y, particleUp.z, engine::ParticleOpacity(p, layer) * fade},
                {color.x, color.y, color.z, 1},
                uv};
    }
    pool.diagnostics.visible = drawItems.size();
    if (drawItems.empty()) return;
    BeginTextureMode(target); BeginMode3D(camera); rlDrawRenderBatchActive();
    // Changing blend mode flushes rlgl's batch and unbinds the GL program,
    // even for an empty batch. Do this before binding our immediate-draw shader.
    rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY);
    rlEnableShader(shader.id);
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    rlSetUniformMatrix(matrixLoc, mvp);
    const int textureUnit = 0, depthUnit = 1, hasDepth = depth && depth->id, display = displaySrgb ? 1 : 0;
    const Vector2 viewport{static_cast<float>(target.texture.width), static_cast<float>(target.texture.height)};
    const Vector2 nearFar{static_cast<float>(rlGetCullDistanceNear()), static_cast<float>(rlGetCullDistanceFar())};
    rlSetUniform(textureLoc, &textureUnit, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(depthLoc, &depthUnit, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(hasDepthLoc, &hasDepth, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(displayLoc, &display, RL_SHADER_UNIFORM_INT, 1);
    rlSetUniform(viewportLoc, &viewport, RL_SHADER_UNIFORM_VEC2, 1);
    rlSetUniform(nearFarLoc, &nearFar, RL_SHADER_UNIFORM_VEC2, 1);
    if (hasDepth) { rlActiveTextureSlot(1); rlEnableTexture(depth->id); }
    rlActiveTextureSlot(0);
    rlDisableDepthMask(); if (hasDepth) rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlEnableVertexArray(vao); rlEnableVertexBuffer(instanceVbo);
    rlUpdateVertexBuffer(instanceVbo, instances.data(), static_cast<int>(drawItems.size() * sizeof(Instance)), 0);
    size_t first = 0;
    while (first < drawItems.size()) {
        size_t end = first + 1;
        while (end < drawItems.size() && drawItems[end].texture == drawItems[first].texture) ++end;
        for (int a = 1; a <= 5; ++a) rlSetVertexAttribute(a, 4, RL_FLOAT, false, sizeof(Instance),
                static_cast<int>(first * sizeof(Instance) + (a - 1) * sizeof(Vector4)));
        rlEnableTexture(drawItems[first].texture);
        rlDrawVertexArrayInstanced(0, 6, static_cast<int>(end - first));
        ++pool.diagnostics.drawCalls; first = end;
    }
    rlDisableVertexBuffer(); rlDisableVertexArray(); rlDisableTexture();
    rlActiveTextureSlot(1); rlDisableTexture(); rlActiveTextureSlot(0);
    rlSetBlendMode(BLEND_ALPHA); rlEnableBackfaceCulling(); rlEnableDepthMask(); rlEnableDepthTest();
    rlDisableShader(); EndMode3D(); EndTextureMode();
    pool.diagnostics.drawCpuMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
} // namespace game
