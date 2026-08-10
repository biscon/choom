#include "engine/assets/ModelAssets.h"
#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(float actual, float expected)
{
    return std::fabs(actual - expected) <= 0.000001f;
}

void TestDiagnosticModesAndScales()
{
    game::SectorPbrContributionSettings settings;
    Check(settings.diagnosticMode == game::SectorPbrDiagnosticMode::Full,
          "PBR diagnostics default to the full renderer");
    Check(Near(settings.worldIndirectDiffuseScale, 1.0f),
          "world indirect diffuse defaults to one");
    Check(Near(settings.worldEnvironmentSpecularScale, 1.0f),
          "world environment specular defaults to one");

    settings.diagnosticMode = static_cast<game::SectorPbrDiagnosticMode>(999);
    settings.worldIndirectDiffuseScale = -2.0f;
    settings.worldEnvironmentSpecularScale = 4.0f;
    settings = game::NormalizeSectorPbrContributionSettings(settings);
    Check(settings.diagnosticMode == game::SectorPbrDiagnosticMode::Full,
          "invalid PBR diagnostic mode falls back to full");
    Check(Near(settings.worldIndirectDiffuseScale, 0.0f),
          "world indirect scale clamps low");
    Check(Near(settings.worldEnvironmentSpecularScale, 1.0f),
          "world environment scale clamps high");

    settings.worldIndirectDiffuseScale =
            std::numeric_limits<float>::quiet_NaN();
    settings.worldEnvironmentSpecularScale =
            std::numeric_limits<float>::infinity();
    settings = game::NormalizeSectorPbrContributionSettings(settings);
    Check(Near(settings.worldIndirectDiffuseScale, 1.0f)
                    && Near(settings.worldEnvironmentSpecularScale, 1.0f),
          "non-finite contribution scales restore safe defaults");
}

void TestIndirectAndEnvironmentRouting()
{
    game::SectorPbrContributionSettings settings;
    settings.diagnosticMode = game::SectorPbrDiagnosticMode::DirectDiffuse;
    settings.worldIndirectDiffuseScale = 0.25f;
    settings.worldEnvironmentSpecularScale = 0.5f;

    const game::SectorPbrDrawState validProbe = game::BuildSectorPbrDrawState(
            game::SectorPbrLightingPath::WorldDynamic,
            true,
            false,
            true,
            0.35f,
            7.0f,
            true,
            settings);
    Check(validProbe.indirectSource == game::SectorPbrIndirectSource::ObjectProbe
                    && validProbe.useObjectProbe
                    && validProbe.useVerticalObjectProbe,
          "valid dynamic-model probe replaces fallback ambient");
    Check(Near(validProbe.indirectDiffuseScale, 0.25f)
                    && Near(validProbe.environmentSpecularScale, 0.5f),
          "world model receives independent contribution scales");
    Check(validProbe.environmentActive
                    && Near(validProbe.environmentExposure, 0.35f),
          "real environment is eligible at its validated exposure");
    Check(Near(validProbe.outputBrightnessMultiplier, 1.0f)
                    && !validProbe.materialOverrideActive,
          "world path rejects viewmodel brightness and overrides");

    const game::SectorPbrDrawState fallback = game::BuildSectorPbrDrawState(
            game::SectorPbrLightingPath::WorldDynamic,
            false,
            false,
            false,
            0.35f,
            1.0f,
            false,
            settings);
    Check(fallback.indirectSource == game::SectorPbrIndirectSource::SectorAmbient
                    && !fallback.useObjectProbe,
          "invalid probe selects sector ambient fallback");
    Check(!fallback.environmentActive
                    && Near(fallback.environmentExposure, 0.0f),
          "missing environment contributes exactly zero");

    const game::SectorPbrDrawState lightmap = game::BuildSectorPbrDrawState(
            game::SectorPbrLightingPath::WorldStatic,
            true,
            true,
            false,
            1.0f,
            1.0f,
            false,
            settings);
    Check(lightmap.indirectSource
                    == game::SectorPbrIndirectSource::StaticLightmap
                    && !lightmap.useObjectProbe,
          "static lightmap source is explicit and does not stack a probe");
}

void TestViewmodelIsolationAndFiniteHandling()
{
    game::SectorPbrContributionSettings settings;
    settings.worldIndirectDiffuseScale = 0.0f;
    settings.worldEnvironmentSpecularScale = 0.25f;
    const game::SectorPbrDrawState viewmodel = game::BuildSectorPbrDrawState(
            game::SectorPbrLightingPath::Viewmodel,
            true,
            false,
            true,
            0.2f,
            0.45f,
            true,
            settings);
    Check(Near(viewmodel.indirectDiffuseScale, 1.0f)
                    && Near(viewmodel.environmentSpecularScale, 1.0f),
          "world contribution tuning does not affect the viewmodel");
    Check(Near(viewmodel.outputBrightnessMultiplier, 0.45f)
                    && viewmodel.materialOverrideActive,
          "viewmodel-only brightness and override remain active");

    const game::SectorPbrDrawState invalid = game::BuildSectorPbrDrawState(
            game::SectorPbrLightingPath::ViewmodelAttachment,
            false,
            false,
            true,
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN(),
            true,
            settings);
    Check(Near(invalid.environmentExposure, 0.0f)
                    && Near(invalid.outputBrightnessMultiplier, 1.0f),
          "non-finite PBR inputs cannot poison HDR output");

    engine::ModelMaterialAsset invalidMaterial;
    invalidMaterial.baseColorFactor.x =
            std::numeric_limits<float>::quiet_NaN();
    invalidMaterial.emissiveFactor.y =
            std::numeric_limits<float>::infinity();
    invalidMaterial.metallicFactor = -1.0f;
    invalidMaterial.roughnessFactor =
            std::numeric_limits<float>::infinity();
    invalidMaterial.occlusionStrength = 2.0f;
    invalidMaterial = game::NormalizeSectorPbrMaterial(invalidMaterial);
    Check(Near(invalidMaterial.baseColorFactor.x, 1.0f)
                    && Near(invalidMaterial.emissiveFactor.y, 0.0f)
                    && Near(invalidMaterial.metallicFactor, 0.0f)
                    && Near(invalidMaterial.roughnessFactor, 1.0f)
                    && Near(invalidMaterial.occlusionStrength, 1.0f),
          "non-finite and out-of-range material inputs are sanitized on CPU");

    game::SectorViewmodelLightingContext overrideLighting;
    overrideLighting.materialOverrideEnabled = true;
    overrideLighting.metallicFactor = 0.7f;
    overrideLighting.roughnessFactor = 0.2f;
    overrideLighting.useMetallicRoughnessTexture = false;
    float worldMetallic = 0.1f;
    float worldRoughness = 0.8f;
    bool worldMetalTexture = true;
    bool worldRoughTexture = true;
    float viewmodelMetallic = worldMetallic;
    float viewmodelRoughness = worldRoughness;
    bool viewmodelMetalTexture = worldMetalTexture;
    bool viewmodelRoughTexture = worldRoughTexture;
    game::ApplySectorViewmodelMaterialOverride(
            overrideLighting,
            viewmodelMetallic,
            viewmodelRoughness,
            viewmodelMetalTexture,
            viewmodelRoughTexture);
    Check(Near(worldMetallic, 0.1f) && Near(worldRoughness, 0.8f)
                    && worldMetalTexture && worldRoughTexture,
          "viewmodel material override does not mutate world material state");
    Check(Near(viewmodelMetallic, 0.7f) && Near(viewmodelRoughness, 0.2f)
                    && !viewmodelMetalTexture && !viewmodelRoughTexture,
          "viewmodel material override applies only to its local copy");
}

void TestMaterialTextureSemantics()
{
    using engine::ModelMaterialTextureRole;
    using engine::ModelTextureTransfer;
    Check(engine::ModelMaterialTextureTransfer(
                  ModelMaterialTextureRole::BaseColor)
                    == ModelTextureTransfer::ExplicitSrgbDecode,
          "base color uses exactly-once sRGB decoding");
    Check(engine::ModelMaterialTextureTransfer(
                  ModelMaterialTextureRole::Emissive)
                    == ModelTextureTransfer::ExplicitSrgbDecode,
          "emissive uses exactly-once sRGB decoding");
    Check(engine::ModelMaterialTextureTransfer(
                  ModelMaterialTextureRole::Metallic)
                    == ModelTextureTransfer::LinearData
                    && engine::ModelMaterialTextureTransfer(
                               ModelMaterialTextureRole::Roughness)
                            == ModelTextureTransfer::LinearData
                    && engine::ModelMaterialTextureTransfer(
                               ModelMaterialTextureRole::Normal)
                            == ModelTextureTransfer::LinearData
                    && engine::ModelMaterialTextureTransfer(
                               ModelMaterialTextureRole::Occlusion)
                            == ModelTextureTransfer::LinearData,
          "scalar, mask, normal, and AO roles remain linear data");
    Check(engine::ModelMaterialPackedSourceChannel(
                  ModelMaterialTextureRole::Metallic) == 2,
          "glTF metallic is routed from packed B");
    Check(engine::ModelMaterialPackedSourceChannel(
                  ModelMaterialTextureRole::Roughness) == 1,
          "glTF roughness is routed from packed G");
    Check(engine::ModelMaterialMapIndex(ModelMaterialTextureRole::Metallic)
                    == MATERIAL_MAP_METALNESS
                    && engine::ModelMaterialMapIndex(
                               ModelMaterialTextureRole::Roughness)
                            == MATERIAL_MAP_ROUGHNESS,
          "split metallic and roughness textures bind their active shader maps");
}

void TestEnvironmentEligibility()
{
    game::SectorPbrEnvironment environment;
    TextureCubemap texture{};
    Check(!game::IsSectorPbrEnvironmentActive(environment, &texture),
          "default environment is inactive");
    environment.active = true;
    environment.cubemap = engine::TextureHandle{1, 1};
    texture.id = 7;
    Check(game::IsSectorPbrEnvironmentActive(environment, &texture),
          "eligible real environment requires active state, handle, and texture");
    environment.active = false;
    Check(!game::IsSectorPbrEnvironmentActive(environment, &texture),
          "cleared eligibility suppresses a still-bound texture");
}

void TestRemovedShaderPathsStayRemoved()
{
    std::ifstream input(PBR_SHADER_SOURCE_PATH);
    const std::string source(
            (std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
    Check(!source.empty(), "PBR shader source guard can read active renderer");
    Check(source.find("roughSpecularFloor") == std::string::npos,
          "synthetic rough specular floor stays removed");
    Check(source.find("uniform float dynamicLightingClamp") == std::string::npos,
          "PBR shader finite upper radiance clamp stays removed");
    Check(source.find("Aces") == std::string::npos
                    && source.find("LinearToSrgb") == std::string::npos,
          "PBR shader does not reintroduce local tone mapping or output transfer");
}

} // namespace

int main()
{
    TestDiagnosticModesAndScales();
    TestIndirectAndEnvironmentRouting();
    TestViewmodelIsolationAndFiniteHandling();
    TestMaterialTextureSemantics();
    TestEnvironmentEligibility();
    TestRemovedShaderPathsStayRemoved();
    if (failures != 0) {
        std::fprintf(stderr, "%d PBR lighting policy test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
