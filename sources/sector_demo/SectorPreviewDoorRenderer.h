#pragma once

#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPreviewDynamicLighting.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

struct SectorBakedObjectLightProbeRuntimeData;
struct SectorDoorShadowCaster;
struct SectorTopologyMap;

struct SectorRuntimeDoorLightingContext {
    const SectorBakedObjectLightProbeRuntimeData* objectLightProbes = nullptr;
    const SectorTopologyMap* mapForFallback = nullptr;
};

struct SectorPreviewDoorRenderStats {
    size_t considered = 0;
    size_t drawn = 0;
    size_t skipped = 0;
};

struct SectorPreviewDoorTextureResolver {
    using ResolveFn = const Texture2D* (*)(
            void* userData,
            engine::AssetManager& assets,
            const std::string& textureId);

    void* userData = nullptr;
    ResolveFn resolve = nullptr;
};

struct SectorPreviewDoorMeshResolver {
    using ResolveFn = const Mesh* (*)(
            void* userData,
            const SectorDoorShadowCaster& caster,
            float& outWidth,
            float& outHeight);

    void* userData = nullptr;
    ResolveFn resolve = nullptr;
};

struct SectorPreviewDoorPreparationContext {
    engine::World* runtimeObjectWorld = nullptr;
    std::vector<SectorDoorShadowCaster>* shadowCasters = nullptr;
};

struct SectorPreviewDoorResourceContext {
    Shader* opaqueShader = nullptr;
    Material* opaqueMaterial = nullptr;
    Texture2D* defaultMaterialTexture = nullptr;
    bool* opaqueShaderLoaded = nullptr;
    bool* opaqueMaterialLoaded = nullptr;
};

struct SectorPreviewDoorDynamicLightContext {
    bool enabled = true;
    float runtimeSeconds = 0.0f;
    const std::vector<SectorPreviewDynamicPointLightUniform>* selectedLights = nullptr;
    SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms{};
    SectorPreviewDynamicShadowMapTextures shadowMaps{};
    float lightingClamp = 4.0f;
};

struct SectorPreviewDoorDrawContext {
    engine::AssetManager* assets = nullptr;
    engine::World* runtimeObjectWorld = nullptr;
    SectorRuntimeDoorLightingContext lighting;
    SectorPreviewDoorDynamicLightContext dynamicLighting;
    SectorPreviewDoorTextureResolver textureResolver;
    std::string* renderDebugText = nullptr;
};

struct SectorPreviewDoorShadowCasterContext {
    const std::vector<SectorDoorShadowCaster>* shadowCasters = nullptr;
    SectorPreviewDoorTextureResolver textureResolver;
    SectorPreviewDoorMeshResolver meshResolver;
};

} // namespace game
