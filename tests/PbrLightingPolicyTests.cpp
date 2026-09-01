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
    Check(static_cast<int>(game::SectorPbrDiagnosticMode::ShadingNormal) == 9
                    && static_cast<int>(
                               game::SectorPbrDiagnosticMode::TangentNormal) == 10,
          "raw tangent-normal diagnostics append without renumbering existing modes");

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
            true,
            0.35f,
            7.0f,
            true,
            settings);
    Check(validProbe.indirectSource == game::SectorPbrIndirectSource::ObjectProbe
                    && validProbe.useObjectProbe
                    && validProbe.useVerticalObjectProbe,
          "valid dynamic-model probe replaces fallback ambient");
    Check(validProbe.staticSpecularEligible,
          "current valid probe enables static-light specular");
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
            false,
            0.35f,
            1.0f,
            false,
            settings);
    Check(fallback.indirectSource == game::SectorPbrIndirectSource::SectorAmbient
                    && !fallback.useObjectProbe,
          "invalid probe selects sector ambient fallback");
    Check(!fallback.staticSpecularEligible,
          "invalid probe disables static-light specular");
    Check(!fallback.environmentActive
                    && Near(fallback.environmentExposure, 0.0f),
          "missing environment contributes exactly zero");

    const game::SectorPbrDrawState lightmap = game::BuildSectorPbrDrawState(
            game::SectorPbrLightingPath::WorldStatic,
            true,
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
    Check(lightmap.staticSpecularEligible,
          "current static lightmap enables static-light specular");

    const game::SectorPbrDrawState staleLightmap =
            game::BuildSectorPbrDrawState(
                    game::SectorPbrLightingPath::WorldStatic,
                    false,
                    true,
                    false,
                    false,
                    0.0f,
                    1.0f,
                    false,
                    settings);
    Check(!staleLightmap.staticSpecularEligible,
          "stale static lightmap disables static-light specular");
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
    invalidMaterial.emissiveStrength =
            std::numeric_limits<float>::infinity();
    invalidMaterial.metallicFactor = -1.0f;
    invalidMaterial.roughnessFactor =
            std::numeric_limits<float>::infinity();
    invalidMaterial.occlusionStrength = 2.0f;
    invalidMaterial = game::NormalizeSectorPbrMaterial(invalidMaterial);
    Check(Near(invalidMaterial.baseColorFactor.x, 1.0f)
                    && Near(invalidMaterial.emissiveFactor.y, 0.0f)
                    && Near(invalidMaterial.emissiveStrength, 1.0f)
                    && Near(invalidMaterial.metallicFactor, 0.0f)
                    && Near(invalidMaterial.roughnessFactor, 1.0f)
                    && Near(invalidMaterial.occlusionStrength, 1.0f),
          "non-finite and out-of-range material inputs are sanitized on CPU");
    invalidMaterial.emissiveStrength=70000.0f;
    invalidMaterial=game::NormalizeSectorPbrMaterial(invalidMaterial);
    Check(Near(invalidMaterial.emissiveStrength,65504.0f),
          "glTF emissive strength is limited only by finite-half storage");
    Check(Near(game::ScaleSectorPbrEmissiveStrength(3.0f, 0.0f), 0.0f)
                    && Near(game::ScaleSectorPbrEmissiveStrength(3.0f, 0.5f), 1.5f)
                    && Near(game::ScaleSectorPbrEmissiveStrength(
                               40000.0f,
                               2.0f),
                               engine::Rgba16fMaximumFinite)
                    && Near(game::ScaleSectorPbrEmissiveStrength(
                               3.0f,
                               std::numeric_limits<float>::quiet_NaN()),
                               3.0f),
          "per-prop emissive scaling supports off, dimming, HDR boost, and finite fallback");

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
    Check(game::SectorStaticModelEnvironmentMaterialMap
                    == MATERIAL_MAP_CUBEMAP
                    && game::SectorStaticModelEnvironmentMaterialMap
                            != MATERIAL_MAP_ALBEDO,
          "the PBR environment has a dedicated cubemap texture unit");
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

std::string ReadSource(const char* path);

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
    Check(source.find(
                      "environmentTextureLocation,\n"
                      "            SectorStaticModelEnvironmentMaterialMap")
                            != std::string::npos
                    && source.find(
                               "InitializeSectorPbrSamplerUnits(\n"
                               "            shader,")
                            != std::string::npos,
          "PBR samplers receive fixed units even when optional textures are absent");
    Check(source.find("#define MAX_STATIC_SPECULAR_LIGHTS 4")
                    != std::string::npos
                    && source.find("staticDirectSpecular +=")
                            != std::string::npos
                    && source.find(
                               "+ dynamicDirectSpecular\n"
                               "            + staticDirectSpecular")
                            != std::string::npos,
          "static authored lights contribute through the bounded direct-specular path");
    Check(source.find("directDiffuse += staticDirectSpecular")
                    == std::string::npos,
          "static authored lights do not duplicate baked diffuse lighting");
    const std::string sectorSource = ReadSource(SECTOR_SHADER_SOURCE_PATH);
    const std::string doorSource = ReadSource(DOOR_SHADER_SOURCE_PATH);
    const auto hasStaticRectFeather = [](const std::string& shaderSource) {
        return shaderSource.find("staticSpecularLightStartFeathers") != std::string::npos
                && shaderSource.find("staticSpecularLightTypes[i] == 2") != std::string::npos
                && shaderSource.find("smoothstep(0.0, startFeather, frontDistance)")
                        != std::string::npos;
    };
    Check(hasStaticRectFeather(source)
                    && hasStaticRectFeather(sectorSource)
                    && hasStaticRectFeather(doorSource),
          "static model, sector, and door specular paths apply rect start feathering");
    const std::size_t emissionShapeStart = source.find(
            "vec3 ShapeModelEmissive(");
    const std::size_t emissionShapeEnd = source.find(
            ")\"\nSECTOR_DYNAMIC_SURFACE_SHADOW_GLSL",
            emissionShapeStart);
    const std::string emissionShape = emissionShapeStart != std::string::npos
                    && emissionShapeEnd != std::string::npos
            ? source.substr(
                    emissionShapeStart,
                    emissionShapeEnd - emissionShapeStart)
            : std::string{};
    Check(!emissionShape.empty()
                    && emissionShape.find(
                               "mix(0.70, 1.0, smoothstep(0.0, 0.60, facing))")
                            != std::string::npos
                    && emissionShape.find(
                               "0.70 * smoothstep(1.0, 4.0, peak)")
                            != std::string::npos
                    && emissionShape.find("vec3(peak)") != std::string::npos
                    && emissionShape.find("roughness") == std::string::npos
                    && emissionShape.find("materialAo") == std::string::npos
                    && emissionShape.find("normalTexture") == std::string::npos,
          "model emission uses fixed branch-free white-core and soft-edge shaping without repurposing PBR textures");
    Check(source.find(
                      "float visibility = DynamicLightShadowVisibility(")
                            != std::string::npos
                    && source.find(
                               "dynamicLightContext.shadowMaps.shadowMap0")
                            != std::string::npos
                    && source.find(
                               "dynamicLightContext.shadowMaps.shadowMap1")
                            != std::string::npos,
          "static and dynamic world-model PBR draws receive dynamic spotlight shadow maps");
    Check(source.find("DrawWorldDynamicModel(") != std::string::npos
                    && source.find("SectorDoorModelRender& modelRender")
                            != std::string::npos
                    && source.find(
                               "SectorPbrLightingPath::WorldDynamic,\n"
                               "                validProbe,\n"
                               "                false,")
                            != std::string::npos,
          "dynamic props and model doors share the non-lightmapped world PBR draw helper");
    const std::size_t itemPass = source.find("SectorItem>(");
    const std::size_t itemHighlightReset = source.find(
            "UploadInteractionHighlightStrength(0.0f);",
            itemPass);
    const std::size_t staticPropPass = source.find(
            "SectorStaticModel>(",
            itemPass);
    Check(itemPass != std::string::npos
                    && itemHighlightReset != std::string::npos
                    && staticPropPass != std::string::npos
                    && itemPass < itemHighlightReset
                    && itemHighlightReset < staticPropPass,
          "item selection highlight is cleared before the static-prop draw pass");
    const std::size_t staticHighlight = source.find(
            "!staticCaptureOnly && entity == useHighlight.entity",
            staticPropPass);
    const std::size_t dynamicPropPass = source.find(
            "SectorDynamicModel,",
            staticPropPass);
    Check(staticHighlight != std::string::npos
                    && dynamicPropPass != std::string::npos
                    && staticHighlight < dynamicPropPass,
          "static props receive per-entity interaction highlighting outside static capture");
}

std::string ReadSource(const char* path)
{
    std::ifstream input(path);
    return std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
}

void TestSectorRuntimeNormalMappingPolicy()
{
    const std::string source = ReadSource(SECTOR_SHADER_SOURCE_PATH);
    const std::string door = ReadSource(DOOR_SHADER_SOURCE_PATH);
    const std::string window = ReadSource(WINDOW_SHADER_SOURCE_PATH);
    Check(!source.empty(),
          "sector runtime normal-mapping policy can read the active renderer");
    Check(source.find("engine::TextureColorUsage::LinearData")
                    != std::string::npos
                    && source.find("normalTextureHandlesById.insert_or_assign(")
                            != std::string::npos,
          "automatic sector normal maps load as linear texture data");
    Check(source.find(
                      "float uvDeterminant = uvDx.x * uvDy.y - uvDx.y * uvDy.x")
                    != std::string::npos
                    && source.find(
                               "uvDeterminant * uvDeterminant\n"
                               "                    <= uvDerivativeScaleSq * 0.00000001")
                            != std::string::npos
                    && source.find("float inverseUvDeterminant = 1.0 / uvDeterminant")
                            != std::string::npos
                    && source.find(
                               "tangent -= geometricNormal * dot(tangent, geometricNormal)")
                    != std::string::npos
                    && source.find(
                               "dot(cross(geometricNormal, tangent), sourceBitangent) < 0.0")
                            != std::string::npos
                    && source.find("cross(geometricNormal, tangent)")
                            != std::string::npos
                    && source.find(
                               "mat3(tangent, bitangent, geometricNormal) * mappedNormal")
                            != std::string::npos,
          "sector runtime normal mapping builds an orthonormal handed tangent basis");
    Check(source.find("inverseBasisLength") == std::string::npos
                    && source.find(
                               "dot(tangent, tangent) <= 0.00000001")
                            == std::string::npos
                    && source.find(
                               "dot(sourceBitangent, sourceBitangent) <= 0.00000001")
                            == std::string::npos,
          "sector tangent validity is independent of screen-derivative magnitude");
    Check(source.find("vec3 worldNormal = SurfaceNormal(\n"
                      "            geometricNormal, tangentNormalSample)")
                    != std::string::npos
                    && source.find("dot(worldNormal, lightDirection)")
                            != std::string::npos,
          "dynamic sector lights evaluate the mapped world normal");
    Check(source.find("uniform sampler2D directionalLightmapTexture")
                    != std::string::npos
                    && source.find("vec3 ApplyDirectionalLightmap(")
                            != std::string::npos
                    && source.find(
                               "mappedResponse / geometricResponse, 0.0, 4.0")
                            != std::string::npos,
          "normal-mapped sector surfaces evaluate bounded directional baked diffuse at runtime");
    Check(source.find("float DistributionGgx(") != std::string::npos
                    && source.find("dynamicDirectSpecular +=")
                            != std::string::npos
                    && source.find("staticDirectSpecular +=")
                            != std::string::npos
                    && source.find("staticSpecularLightCount")
                            != std::string::npos,
          "sector dynamic and bounded authored-static lights use GGX specular");
    Check(source.find("metallicFactorById.insert_or_assign") != std::string::npos
                    && source.find("roughnessFactorById.insert_or_assign")
                            != std::string::npos
                    && source.find("mix(vec3(0.04), surfaceRgb, metallic)")
                            != std::string::npos
                    && source.find("textureLod(\n"
                               "                environmentTexture")
                            != std::string::npos,
          "sector material scalars drive metallic-roughness shading and environment specular");
    Check(source.find("InitializeSectorSurfaceSamplerUnits(material.shader)")
                    != std::string::npos
                    && source.find(
                               "for (int textureUnit = MATERIAL_MAP_ALBEDO;")
                            != std::string::npos,
          "sector material samplers receive deterministic fixed texture units");
    Check(source.find(
                      "surfaceLightmapBakeCurrent && !staticCaptureOnly)")
                            != std::string::npos
                    && source.find(
                               "doorDrawContext.staticSpecularEligible = !staticCaptureOnly")
                            != std::string::npos
                    && source.find(
                               "!staticCaptureOnly && objectProbeBakeCurrent")
                            != std::string::npos,
          "reflection probe captures exclude view-dependent direct specular from sectors and objects");

    const std::string model = ReadSource(PBR_SHADER_SOURCE_PATH);
    Check(source.find("pbrDiagnosticMode == 10") != std::string::npos
                    && model.find("pbrDiagnosticMode == 10")
                            != std::string::npos
                    && source.find("? tangentNormalSample\n"
                               "                : vec3(1.0, 0.0, 1.0)")
                            != std::string::npos
                    && model.find("? tangentNormalSample\n"
                               "                : vec3(1.0, 0.0, 1.0)")
                            != std::string::npos,
          "sector and model raw-normal diagnostics show sampled RGB or magenta when absent");
    Check(source.find("if (pbrDiagnosticMode == 0) {\n"
                      "        surfaceOutput = ApplySectorFog(")
                    != std::string::npos,
          "sector PBR diagnostics bypass fog while full rendering retains it");
    Check(door.find("uniform sampler2D normalTexture") != std::string::npos
                    && door.find("float uvDeterminant = uvDx.x * uvDy.y - uvDx.y * uvDy.x")
                            != std::string::npos
                    && door.find("mat3(tangent, bitangent, geometricNormal) * mappedNormal")
                            != std::string::npos,
          "procedural doors apply OpenGL tangent-space normal maps at runtime");
    Check(door.find("float DistributionGgx(") != std::string::npos
                    && door.find("dynamicDirectSpecular +=") != std::string::npos
                    && door.find("staticDirectSpecular +=") != std::string::npos
                    && door.find("mix(vec3(0.04), surfaceRgb, metallic)")
                            != std::string::npos
                    && door.find("textureLod(\n"
                               "                environmentTexture")
                            != std::string::npos,
          "procedural door material scalars drive dynamic static and environment GGX lighting");
    Check(door.find("pbrDiagnosticMode == 8") != std::string::npos
                    && door.find("pbrDiagnosticMode == 9") != std::string::npos
                    && door.find("pbrDiagnosticMode == 10") != std::string::npos
                    && door.find("doorDebugMode") == std::string::npos
                    && door.find("DOOR_DEBUG_") == std::string::npos,
          "procedural doors use shared PBR diagnostics without the legacy door-only modes");
    Check(door.find("if (pbrDiagnosticMode == 0) {\n"
                      "        outputRgb = ApplySectorFog(")
                    != std::string::npos,
          "door PBR diagnostics bypass fog while full rendering retains it");
    Check(!window.empty()
                    && window.find("float DistributionGgx(")
                            != std::string::npos
                    && window.find("float GeometrySmith(")
                            != std::string::npos
                    && window.find("float FresnelSchlick(")
                            != std::string::npos
                    && window.find(
                               "float specular = distribution * geometry * fresnel")
                            != std::string::npos
                    && window.find("mix(256.0, 4.0, roughness)")
                            == std::string::npos,
          "procedural windows use normalized GGX direct specular instead of a constant-peak exponent lobe");
    Check(source.find("NormalMappedRendererMaterialIds(map, geometry)")
                            != std::string::npos
                    && source.find("ResolveDoorMaterial(") != std::string::npos
                    && source.find("normalTextureHandlesById.find(materialId)")
                            != std::string::npos,
          "procedural door materials resolve their global normal and scalar metadata");
}

void TestBakedHdrConsumersStayUnclamped()
{
    const std::string sector = ReadSource(SECTOR_SHADER_SOURCE_PATH);
    const std::string door = ReadSource(DOOR_SHADER_SOURCE_PATH);
    const std::string billboard = ReadSource(BILLBOARD_SHADER_SOURCE_PATH);
    Check(!sector.empty() && !door.empty() && !billboard.empty(),
          "baked HDR shader policy can read every active consumer");
    Check(sector.find("clamp(ambient + bakedDirect, 0.0, 1.0)")
                    == std::string::npos
                    && sector.find("clamp(bakedLighting + dynamicDirect")
                            == std::string::npos,
          "sector baked illumination has no LDR or finite upper clamp");
    Check(door.find("clamp(fragColor.rgb, 0.0, 1.0)") == std::string::npos
                    && door.find("clamp(staticProbeLighting + dynamicDirect")
                            == std::string::npos,
          "door probe illumination remains float HDR through composition");
    Check(billboard.find("clamp(bakedBillboardLighting + dynamicDirect")
                    == std::string::npos,
          "billboard probe illumination has no finite upper clamp");
    Check(sector.find("LinearToSrgb") == std::string::npos
                    && door.find("LinearToSrgb") == std::string::npos
                    && billboard.find("LinearToSrgb") == std::string::npos
                    && sector.find("Aces") == std::string::npos
                    && door.find("Aces") == std::string::npos
                    && billboard.find("Aces") == std::string::npos,
          "baked-light consumers do not tone map or encode output locally");
}

void TestDistanceFogUsesDarknessGatedScattering()
{
    const std::string sector = ReadSource(SECTOR_SHADER_SOURCE_PATH);
    const std::string door = ReadSource(DOOR_SHADER_SOURCE_PATH);
    const std::string billboard = ReadSource(BILLBOARD_SHADER_SOURCE_PATH);
    const std::string model = ReadSource(PBR_SHADER_SOURCE_PATH);
    const std::string lightPeak =
            "float fogLightPeak = max(max(fogLighting.r, fogLighting.g), fogLighting.b)";
    const std::string visibility =
            "float fogLightVisibility = smoothstep(0.0, 0.04, fogLightPeak)";
    const std::string normalizedTint =
            "clamp(fogLighting / fogLightPeak, vec3(0.0), vec3(1.0))";
    const std::string gatedScattering =
            "fogColor * fogLightTint * fogLightVisibility";
    const std::string extinctionAndScattering =
            "surfaceRgb * (1.0 - fogAmount) + fogScattering * fogAmount";
    const std::string directlyScaledScattering =
            "fogColor * max(staticAtmosphericLighting, vec3(0.0))";

    Check(!sector.empty() && !door.empty() && !billboard.empty() && !model.empty(),
          "distance-fog policy can read every active material renderer");
    Check(sector.find(lightPeak) != std::string::npos
                    && door.find(lightPeak) != std::string::npos
                    && billboard.find(lightPeak) != std::string::npos
                    && model.find(lightPeak) != std::string::npos
                    && sector.find(visibility) != std::string::npos
                    && door.find(visibility) != std::string::npos
                    && billboard.find(visibility) != std::string::npos
                    && model.find(visibility) != std::string::npos
                    && sector.find(normalizedTint) != std::string::npos
                    && door.find(normalizedTint) != std::string::npos
                    && billboard.find(normalizedTint) != std::string::npos
                    && model.find(normalizedTint) != std::string::npos
                    && sector.find(gatedScattering) != std::string::npos
                    && door.find(gatedScattering) != std::string::npos
                    && billboard.find(gatedScattering) != std::string::npos
                    && model.find(gatedScattering) != std::string::npos,
          "distance fog normalizes static-light tint and fades scattering near black");
    Check(sector.find("fogLightPeak > 0.00001") != std::string::npos
                    && door.find("fogLightPeak > 0.00001") != std::string::npos
                    && billboard.find("fogLightPeak > 0.00001") != std::string::npos
                    && model.find("fogLightPeak > 0.00001") != std::string::npos
                    && sector.find("            : vec3(0.0);") != std::string::npos
                    && door.find("            : vec3(0.0);") != std::string::npos
                    && billboard.find("            : vec3(0.0);") != std::string::npos
                    && model.find("            : vec3(0.0);") != std::string::npos,
          "distance fog produces no in-scattering for black static illumination");
    Check(sector.find(extinctionAndScattering) != std::string::npos
                    && door.find(extinctionAndScattering) != std::string::npos
                    && billboard.find(extinctionAndScattering) != std::string::npos
                    && model.find(extinctionAndScattering) != std::string::npos,
          "distance fog separates extinction from statically illuminated in-scattering");
    Check(sector.find(directlyScaledScattering) == std::string::npos
                    && door.find(directlyScaledScattering) == std::string::npos
                    && billboard.find(directlyScaledScattering) == std::string::npos
                    && model.find(directlyScaledScattering) == std::string::npos
                    && sector.find("mix(surfaceRgb, fogColor, fogAmount)") == std::string::npos
                    && door.find("mix(surfaceRgb, fogColor, fogAmount)") == std::string::npos
                    && billboard.find("mix(surfaceRgb, fogColor, fogAmount)") == std::string::npos
                    && model.find("mix(surfaceRgb, fogColor, fogAmount)") == std::string::npos,
          "direct light-intensity scaling and constant emissive fog mixing stay removed");
    Check(sector.find(
                      "fragColor.rgb + bakedSample.rgb, vec3(0.0)")
                            != std::string::npos
                    && sector.find(
                               "surfaceOutput = ApplySectorFog(\n"
                               "                surfaceOutput,\n"
                               "                staticAtmosphericLighting,")
                            != std::string::npos,
          "sector fog uses ambient and uncorrected baked light without baked AO or dynamic light");
    Check(door.find(
                      "outputRgb = ApplySectorFog(\n"
                      "                outputRgb,\n"
                      "                staticProbeLighting,")
                            != std::string::npos
                    && billboard.find(
                               "ApplySectorFog(\n"
                               "            surfaceRgb * lighting,\n"
                               "            bakedBillboardLighting,")
                            != std::string::npos,
          "doors and billboards use their existing baked probe lighting for fog");
    Check(model.find("vec3 EvaluateFogObjectProbeLighting()") != std::string::npos
                    && model.find(
                               "staticAtmosphericLighting = containingSectorAmbient + bakedStaticSample.rgb")
                            != std::string::npos
                    && model.find(
                               "ApplySectorFog(\n"
                               "                linearColor,\n"
                               "                staticAtmosphericLighting,")
                            != std::string::npos,
          "world models use orientation-independent probes or ambient plus baked RGB for fog");
}

void TestHdrEffectShaderAndPassPolicies()
{
    const std::string bloom=ReadSource(BLOOM_SHADER_SOURCE_PATH);
    const std::string distanceFog=ReadSource(DISTANCE_FOG_SHADER_SOURCE_PATH);
    const std::string analyticFog=ReadSource(ANALYTIC_FOG_SHADER_SOURCE_PATH);
    const std::string analyticShaft=ReadSource(ANALYTIC_SHAFT_SHADER_SOURCE_PATH);
    const std::string lightProxy=ReadSource(LIGHT_PROXY_SHADER_SOURCE_PATH);
    const std::string dust=ReadSource(DUST_SHADER_SOURCE_PATH);
    const std::string muzzle=ReadSource(MUZZLE_SHADER_SOURCE_PATH);
    const std::string mainGraph=ReadSource(MAIN_RENDER_GRAPH_SOURCE_PATH);
    const std::string modelAssets=ReadSource(MODEL_ASSET_SOURCE_PATH);
    const std::string dynamicModelShadows=ReadSource(DYNAMIC_MODEL_SHADOW_SOURCE_PATH);
    const std::string dynamicLightingShadows=ReadSource(
            DYNAMIC_LIGHTING_SHADOW_SOURCE_PATH);
    const std::string dynamicShadowSampling=ReadSource(
            DYNAMIC_SHADOW_SAMPLING_SOURCE_PATH);
    Check(!bloom.empty()&&!distanceFog.empty()&&!analyticFog.empty()
                    &&!analyticShaft.empty()&&!lightProxy.empty()&&!dust.empty()
                    &&!muzzle.empty()&&!mainGraph.empty()&&!dynamicModelShadows.empty()
                    &&!dynamicLightingShadows.empty()
                    &&!dynamicShadowSampling.empty(),
          "HDR effect policy can read every affected shader and pass graph");
    Check(distanceFog.find("for (") == std::string::npos
                    && analyticFog.find("for (int stepIndex") == std::string::npos
                    && lightProxy.find("for (int lightIndex") == std::string::npos
                    && analyticShaft.find("for (int stepIndex") == std::string::npos
                    && distanceFog.find("uniform sampler2D sceneDepth") != std::string::npos
                    && analyticFog.find("intersectEllipsoid") != std::string::npos
                    && lightProxy.find("intersectSphere") != std::string::npos
                    && lightProxy.find("rlEnableScissorTest") != std::string::npos
                    && analyticShaft.find("intersectFiniteCone") != std::string::npos
                    && analyticShaft.find("rlEnableScissorTest") != std::string::npos
                    && analyticShaft.find("BeginBlendMode(BLEND_ALPHA_PREMULTIPLY)") != std::string::npos
                    && lightProxy.find("BeginBlendMode(BLEND_ALPHA_PREMULTIPLY)") != std::string::npos,
          "atmosphere paths use scissored closed-form work and premultiplied compositing");
    Check(analyticFog.find("ShouldDrawRuntimeSectorForVisibility(")
                            != std::string::npos
                    && analyticFog.find("volume.topologySectorId, visibility")
                            != std::string::npos,
          "local fog candidates are culled by their runtime-visible owner sector");
    Check(lightProxy.find("float visibleChord = max(exitT - enterT, 0.0);")
                            != std::string::npos
                    && lightProxy.find("float opticalThickness = 1.0 - exp(")
                            != std::string::npos
                    && lightProxy.find("0.35 * broad + 0.65 * core") != std::string::npos
                    && lightProxy.find("haloRadiance * scatterWeight") != std::string::npos
                    && lightProxy.find("float extinction = clamp(haloParams.y") != std::string::npos
                    && lightProxy.find("proxy.halo.maxExtinction <= 0.0f") == std::string::npos
                    && lightProxy.find("mappedSoftness = clamp(haloParams.x * 2.0")
                            != std::string::npos
                    && lightProxy.find("DrawMesh") == std::string::npos
                    && lightProxy.find("proxy.shaft") == std::string::npos
                    && analyticShaft.find("float shaftOpticalProfileAt(")
                            != std::string::npos
                    && analyticShaft.find("bool intersectRectFrustum(")
                            != std::string::npos
                    && analyticShaft.find("intersectRectPrism")
                            == std::string::npos
                    && analyticShaft.find(
                            "vec2 sideSlope = (farHalfSize - nearHalfSize) / length;")
                            != std::string::npos
                    && analyticShaft.find(
                            "mix(rectNearHalfSize, rectFarHalfSize, axial01)")
                            != std::string::npos
                    && analyticShaft.find("float rectShaftDensityAt(")
                            != std::string::npos
                    && analyticShaft.find("(shaftParams.x - 0.01) / 0.99")
                            != std::string::npos
                    && analyticShaft.find("mix(0.02, 0.45, rectSoftness)")
                            != std::string::npos
                    && analyticShaft.find("mix(24.0, 4.0, rectSoftness)")
                            != std::string::npos
                    && analyticShaft.find("0.1184634430 * rectShaftDensityAt(")
                            != std::string::npos
                    && analyticShaft.find("chord * 0.9530899230")
                            != std::string::npos
                    && analyticShaft.find(
                            "0.8646647168 * pathCoverage * integratedDensity")
                            != std::string::npos
                    && analyticShaft.find("Vector2 RectShaftFarHalfSize(")
                            != std::string::npos
                    && analyticShaft.find("SpreadDegreesAtScaleOne = 15.0f")
                            != std::string::npos
                    && analyticShaft.find("float coverage = smoothstep(0.0, 1.0, rawCoverage);")
                            != std::string::npos
                    && analyticShaft.find("chord * (1.0 / 6.0)")
                            != std::string::npos
                    && analyticShaft.find("chord * (5.0 / 6.0)")
                            != std::string::npos
                    && analyticShaft.find("float coverage = clamp(chord / localDiameter")
                            == std::string::npos
                    && analyticShaft.find("mappedSoftness = clamp(shaftParams.x * 2.0")
                            != std::string::npos
                    && analyticShaft.find("+ 2.0 * extraSoftness") != std::string::npos
                    && analyticShaft.find("shaftRadiance * scatterWeight") != std::string::npos
                    && analyticShaft.find("float extinction = clamp(shaftParams.y") != std::string::npos
                    && analyticShaft.find("settings.maxExtinction <= 0.0f") == std::string::npos
                    && analyticShaft.find(
                            "rlDrawRenderBatchActive();\n        SetShaderValueTexture(shader, sceneDepthLoc, sceneTarget.depth);")
                            != std::string::npos
                    && analyticFog.find(
                            "rlDrawRenderBatchActive();\n        SetShaderValueTexture(shader, sceneDepthLoc, sceneTarget.depth);")
                            != std::string::npos
                    && lightProxy.find(
                            "rlDrawRenderBatchActive();\n        SetShaderValueTexture(shader, sceneDepthLoc, sceneTarget.depth);")
                            != std::string::npos,
          "fog, haze, and shaft effects preserve depth bindings across render-batch flushes");
    Check(analyticFog.find("bool intersectBox(") != std::string::npos
                    && analyticFog.find("uniform int fogShape") != std::string::npos
                    && analyticFog.find("uniform int fogStyle") != std::string::npos
                    && analyticFog.find("float cloudyBoundary(") != std::string::npos
                    && analyticFog.find("float roomBoundary(") != std::string::npos
                    && analyticFog.find("float ellipsoidDistance(") != std::string::npos
                    && analyticFog.find("float analyticShapeDistance(") != std::string::npos
                    && analyticFog.find("uniform vec2 fogEdgeParams") != std::string::npos
                    && analyticFog.find("intersectionRadii = fogRadii")
                            != std::string::npos
                    && analyticFog.find("mix(enterT, exitT, 0.20)") != std::string::npos
                    && analyticFog.find("mix(enterT, exitT, 0.80)") != std::string::npos
                    && analyticFog.find("nearNoise * 0.25") != std::string::npos
                    && analyticFog.find("middleNoise * 0.50") != std::string::npos
                    && analyticFog.find("farNoise * 0.25") != std::string::npos
                    && analyticFog.find("mix(0.80, 1.20") != std::string::npos
                    && analyticFog.find("valueNoise(noisePosition / max(noiseScale, 0.05))")
                            != std::string::npos
                    && analyticFog.find("float silhouetteScale = max(")
                            != std::string::npos
                    && analyticFog.find("sampleFogNoiseAtScale(")
                            != std::string::npos
                    && analyticFog.find("float shapedPath = chord * boundary * noiseModulation;")
                            != std::string::npos
                    && analyticFog.find("float projectedNoisePixels") != std::string::npos
                    && analyticFog.find("float projectedMinimumDiameterPixels")
                            != std::string::npos
                    && analyticFog.find("nearNoise = mix(0.5, nearNoise, noiseDetail);")
                            != std::string::npos
                    && analyticFog.find("middleNoise = mix(0.5, middleNoise, noiseDetail);")
                            != std::string::npos
                    && analyticFog.find("farNoise = mix(0.5, farNoise, noiseDetail);")
                            != std::string::npos
                    && analyticFog.find("pow(pathProfile, max(fogParams.z, 0.0001))")
                            != std::string::npos
                    && analyticFog.find("float peakAttenuation")
                            != std::string::npos
                    && analyticFog.find("float smallVolumeVisibility")
                            != std::string::npos
                    && analyticFog.find("authoredExponent * 0.55") == std::string::npos
                    && analyticFog.find("ComputeSectorAnalyticFogCloudyEdgeExpansion(")
                            != std::string::npos
                    && analyticFog.find("fogColor * staticLighting") != std::string::npos
                    && analyticFog.find("SampleSectorLocalFogStaticLighting(")
                            != std::string::npos
                    && analyticFog.find("BeginBlendMode(BLEND_ALPHA);")
                            != std::string::npos
                    && analyticFog.find("BeginBlendMode(BLEND_ADDITIVE);")
                            == std::string::npos
                    && analyticFog.find("dynamicLightCount") == std::string::npos
                    && analyticFog.find("for (int stepIndex") == std::string::npos,
          "fog separates shape from style with feathered noisy silhouettes, filtered fixed interior taps, conservative bounds, alpha blending, and cached baked lighting without marching or dynamic-light loops");
    Check(bloom.find("Rgba8Unorm")==std::string::npos
                    && analyticFog.find("Rgba8Unorm")==std::string::npos
                    && analyticShaft.find("Rgba8Unorm")==std::string::npos
                    && lightProxy.find("Rgba8Unorm")==std::string::npos
                    && dust.find("Rgba8Unorm")==std::string::npos,
          "radiance-bearing atmosphere and bloom targets never select RGBA8");
    Check(analyticFog.find("dynamicLightingClamp")==std::string::npos
                    && analyticShaft.find("dynamicLightingClamp")==std::string::npos
                    && lightProxy.find("dynamicLightingClamp")==std::string::npos
                    && dust.find("dynamicLightingClamp")==std::string::npos,
          "obsolete dynamic-light artistic ceilings stay removed from atmosphere");
    Check(bloom.find("65504.0")!=std::string::npos
                    && analyticFog.find("65504.0")!=std::string::npos
                    && analyticShaft.find("65504.0")!=std::string::npos
                    && lightProxy.find("65504.0")!=std::string::npos
                    && dust.find("65504.0")!=std::string::npos
                    && muzzle.find("65504.0")!=std::string::npos,
          "affected RGBA16F writes retain the named finite-half storage guard");
    Check(bloom.find("finalColor = vec4(SanitizeLinearHdrForRgba16f(color), 0.0)")
                    !=std::string::npos
                    && bloom.find("SafeAlpha(scene.a)")!=std::string::npos,
          "bloom buffers ignore alpha energy and composition preserves scene alpha");
    Check(muzzle.find("srgbToLinear")!=std::string::npos
                    && muzzle.find("radianceStrength")!=std::string::npos
                    && muzzle.find("BeginBlendMode(BLEND_ADD_COLORS)")!=std::string::npos
                    && muzzle.find("rlDrawRenderBatchActive")!=std::string::npos,
          "muzzle swatches decode once and add data-driven HDR radiance with synchronized state");
    Check(modelAssets.find("has_emissive_strength")!=std::string::npos
                    && ReadSource(PBR_SHADER_SOURCE_PATH).find(
                               "emissive *= max(emissiveStrength, 0.0)")
                            !=std::string::npos,
          "core glTF emissive strength reaches per-fragment HDR emission");
    Check(ReadSource(PBR_SHADER_SOURCE_PATH).find("wholeModelBloom")
                    ==std::string::npos,
          "models are not tagged as whole-object bloom sources");
    Check(bloom.find("LinearToSrgb")==std::string::npos
                    && analyticFog.find("LinearToSrgb")==std::string::npos
                    && analyticShaft.find("LinearToSrgb")==std::string::npos
                    && lightProxy.find("LinearToSrgb")==std::string::npos
                    && dust.find("LinearToSrgb")==std::string::npos
                    && muzzle.find("LinearToSrgb")==std::string::npos
                    && bloom.find("ToneMap")==std::string::npos,
          "effect shaders do not tone map or output-encode radiance locally");
    Check(bloom.find("ApplyEmissiveDecalBloom")==std::string::npos
                    && ReadSource(SECTOR_SHADER_SOURCE_PATH).find(
                               "emissiveRadiance * emissiveDecalAlpha")!=std::string::npos
                    && ReadSource(SECTOR_SHADER_SOURCE_PATH).find(
                               "surfaceRgb = mix(baseColor.rgb, decalRgb, decalAlpha)")
                            !=std::string::npos,
          "decal-only bloom redraw is retired in favor of visible emissive radiance");
    const std::size_t atmosphere=mainGraph.find("Apply3DWorldAtmosphere");
    const std::size_t glass=mainGraph.find("Apply3DGlass");
    const std::size_t viewmodel=mainGraph.find("Render3DViewmodel");
    const std::size_t sceneBloom=mainGraph.find("Apply3DHdrBloom");
    const std::size_t overlays=mainGraph.find("Render3DOverlays");
    Check(glass<atmosphere&&atmosphere<viewmodel&&viewmodel<sceneBloom&&sceneBloom<overlays,
          "pass graph orders glass, atmosphere, viewmodel, bloom, then excluded editor overlays");
    const std::string glassShader=ReadSource(WINDOW_SHADER_SOURCE_PATH);
    Check(glassShader.find("uniform sampler2D sceneColor")!=std::string::npos
                    &&glassShader.find("uniform sampler2D sceneDepth")!=std::string::npos
                    &&glassShader.find("refract(incident, facingNormal, 1.0 / ior)")
                            !=std::string::npos
                    &&glassShader.find("environmentBoxProjection")!=std::string::npos
                    &&glassShader.find("probe.topologySectorId != viewerSectorId")
                            !=std::string::npos
                    &&glassShader.find("shader.locs[SHADER_LOC_MAP_DIFFUSE] = sceneColorLoc")
                            !=std::string::npos
                    &&glassShader.find("shader.locs[SHADER_LOC_MAP_SPECULAR] = sceneDepthLoc")
                            !=std::string::npos
                    &&glassShader.find("gl_FragCoord.z > opaqueDepth")
                            !=std::string::npos
                    &&glassShader.find("SetShaderValueTexture")
                            ==std::string::npos
                    &&glassShader.find("advancedTransmission")!=std::string::npos,
          "the retained refraction backend uses depth-aware scene transmission and box-projected probes");
    Check(glassShader.find("rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY)")
                            !=std::string::npos
                    &&glassShader.find("float fresnel = clamp(0.04 + 0.96")
                            !=std::string::npos
                    &&glassShader.find("float blocker = clamp(mix(opacity, 1.0, fresnel)")
                            !=std::string::npos
                    &&glassShader.find("vec3 tintFilter = mix(vec3(1.0)")
                            !=std::string::npos
                    &&glassShader.find("(1.0 - blocker) * tintFilter")
                            !=std::string::npos
                    &&glassShader.find("reflection * shadingFresnel")!=std::string::npos
                    &&glassShader.find("glassTint * opacity")
                            ==std::string::npos
                    &&glassShader.find("RL_ZERO, RL_SRC_COLOR")
                            !=std::string::npos
                    &&glassShader.find("RL_ONE, RL_ONE")
                            !=std::string::npos,
          "flat glass multiplicatively filters transmission before adding clamped-Fresnel reflection");
    const std::size_t flatTransmissionReturn = glassShader.find(
            "finalColor = vec4(transmission, 1.0)");
    const std::size_t surfaceDetailEvaluation = glassShader.find(
            "GlassSurfaceDetail(normal, shadingNormal, hazeVariation)");
    Check(glassShader.find("fragLocalPosition")!=std::string::npos
                    &&glassShader.find("glassDimensions")!=std::string::npos
                    &&glassShader.find("glassPatternSeed")!=std::string::npos
                    &&glassShader.find("GlassSurfacePattern")!=std::string::npos
                    &&glassShader.find("0.069926812 * strength")
                            !=std::string::npos
                    &&glassShader.find("hazeVariation")!=std::string::npos
                    &&glassShader.find("hazeWeight = min(")!=std::string::npos
                    &&glassShader.find("hazeVariation, 0.20)")
                            !=std::string::npos
                    &&flatTransmissionReturn<surfaceDetailEvaluation,
          "glass surface detail is stable pane-local noise with bounded normal and haze strength, evaluated after cheap flat transmission");
    Check(glassShader.find("DirectionalSpecular")!=std::string::npos
                    &&glassShader.find("ndotl <= 0.0 || ndotv <= 0.0")
                            !=std::string::npos
                    &&glassShader.find("dynamicLightCount")==std::string::npos
                    &&glassShader.find("staticSpecularLightCount")==std::string::npos,
          "glass only receives front-lit directional direct specular");
    Check(mainGraph.find("Render3DHud")>sceneBloom,
          "HUD and ordinary UI are downstream of scene-wide bloom");
    Check(mainGraph.find("rlLoadFramebuffer()")!=std::string::npos
                    &&mainGraph.find("rlLoadTextureDepth(width, height, true)")
                            !=std::string::npos
                    &&mainGraph.find("world.native.texture.id")
                            !=std::string::npos
                    &&mainGraph.find("glClear(GL_DEPTH_BUFFER_BIT)")
                            !=std::string::npos
                    &&mainGraph.find("application.Composite3DViewmodel")
                            ==std::string::npos,
          "viewmodel keeps private depth while drawing directly into shared HDR color");
    const std::string sectorRenderer=ReadSource(SECTOR_SHADER_SOURCE_PATH);
    Check(sectorRenderer.find("glBlitFramebuffer")!=std::string::npos
                    &&sectorRenderer.find("EnsureHdrSceneColorView(sceneTarget)")
                            !=std::string::npos
                    &&sectorRenderer.find("? hdrSceneColorView : sceneTarget.native")
                            !=std::string::npos,
          "advanced glass snapshots color without flipping and renders through the depth-detached scene color view");
    Check(sectorRenderer.find("bool requestRefraction")!=std::string::npos
                    &&sectorRenderer.find("bool refractionReady = requestRefraction")
                            !=std::string::npos
                    &&sectorRenderer.find("preGlassLightEffectsRendered = true")
                            !=std::string::npos
                    &&sectorRenderer.find("dynamicLightState.LightingVisibility()")
                            !=std::string::npos,
          "flat windows skip refraction snapshots while halos and shafts use stable blocker-aware visibility before glass");
    Check(sectorRenderer.find("uniform sampler2D sourceDepth")==std::string::npos
                    &&sectorRenderer.find("float coverage=isnan(source.a)")
                            !=std::string::npos
                    &&ReadSource(PBR_SHADER_SOURCE_PATH).find(
                               "uniform float modelOpacity")
                            !=std::string::npos
                    &&ReadSource(PBR_SHADER_SOURCE_PATH).find(
                               "clamp(modelOpacity, 0.0, 1.0)")
                            !=std::string::npos
                    &&ReadSource(PBR_SHADER_SOURCE_PATH).find(
                               "const float opaque = 1.0f")
                            !=std::string::npos,
          "PBR model opacity supports corpse fading while viewmodels explicitly remain opaque");
    Check(muzzle.find("finalColor=vec4(storeFiniteHalfRadiance(radiance),0.0)")
                            !=std::string::npos
                    &&muzzle.find("BeginBlendMode(BLEND_ADD_COLORS)")
                            !=std::string::npos,
          "muzzle keeps alpha-zero additive HDR semantics under direct viewmodel rendering");
    const std::size_t rgbOnlyShadowMask=dynamicModelShadows.find(
            "rlColorMask(true, true, true, false)");
    const std::size_t contactShadowDraw=dynamicModelShadows.find(
            "DrawContactShadows(context)",rgbOnlyShadowMask);
    const std::size_t restoredShadowMask=dynamicModelShadows.find(
            "rlColorMask(true, true, true, true)",contactShadowDraw);
    Check(dynamicModelShadows.find("rlDrawRenderBatchActive")!=std::string::npos
                    &&dynamicModelShadows.find("rlActiveTextureSlot(0)")
                            !=std::string::npos
                    &&dynamicModelShadows.find("rlEnableColorBlend")
                            !=std::string::npos
                    &&dynamicModelShadows.find("rlSetBlendMode(BLEND_ALPHA)")
                            !=std::string::npos
                    &&dynamicModelShadows.find("rlEnableDepthMask")
                            !=std::string::npos
                    &&dynamicModelShadows.find("rlDisableShader")
                            !=std::string::npos
                    &&rgbOnlyShadowMask!=std::string::npos
                    &&contactShadowDraw!=std::string::npos
                    &&restoredShadowMask!=std::string::npos,
          "dynamic contact shadows blend RGB without corrupting scene alpha and restore draw state");
    Check(dynamicModelShadows.find("DrawProjectedShadows")==std::string::npos
                    &&dynamicModelShadows.find("ProjectedSilhouette")
                            ==std::string::npos
                    &&dynamicModelShadows.find(
                               "!= SectorDynamicModelShadowMode::Contact")
                            !=std::string::npos,
          "contact shadows are mode-exclusive and the projected silhouette pass is removed");
    const std::size_t firstSkinnedShadowVertex=dynamicLightingShadows.find(
            "in vec4 vertexBoneIndices;");
    Check(firstSkinnedShadowVertex!=std::string::npos
                    &&dynamicLightingShadows.find(
                               "SectorSpotLightShadowVs, SectorSpotLightShadowOpaqueFs")
                            !=std::string::npos
                    &&dynamicLightingShadows.find(
                               "SectorSpotLightShadowVs, SectorSpotLightShadowCutoutFs")
                            !=std::string::npos
                    &&dynamicLightingShadows.find("BuildAnimatedModelPoseView")
                            !=std::string::npos
                    &&dynamicLightingShadows.find(
                               "AnimatedModelMeshBoneMatrices")
                            !=std::string::npos
                    &&dynamicLightingShadows.find("dynamicModelShadowCasters")
                            !=std::string::npos
                    &&dynamicLightingShadows.find("contentFingerprint")
                            !=std::string::npos,
          "shared spotlight and point-light atlas passes render and invalidate posed dynamic-model casters");
    Check(dynamicLightingShadows.find("LoadGeometryShader")
                            ==std::string::npos
                    &&dynamicLightingShadows.find("GL_GEOMETRY_SHADER")
                            ==std::string::npos
                    &&dynamicLightingShadows.find(
                               "DynamicPointLightShadowFaceCount")
                            !=std::string::npos
                    &&dynamicLightingShadows.find(
                               "effectiveShadowMapResolution")
                            !=std::string::npos,
          "point-light shadows use six ordinary planar atlas passes without a geometry shader");
    Check(dynamicLightingShadows.find("DynamicRectLightShadowFaceCount")
                            !=std::string::npos
                    &&dynamicLightingShadows.find(
                               "SectorPreviewDynamicLightKind::Rect")
                            !=std::string::npos
                    &&dynamicLightingShadows.find("rectFaceDirections")
                            !=std::string::npos,
          "rect-light shadows use five oriented planar atlas faces");
    Check(dynamicLightingShadows.find("GL_MAX_TEXTURE_SIZE")
                            !=std::string::npos
                    &&dynamicLightingShadows.find(
                               "DynamicShadowAtlasLowResolution")
                            !=std::string::npos
                    &&dynamicLightingShadows.find("falling back to")
                            !=std::string::npos,
          "high-quality dynamic shadow allocation checks GPU limits and exposes the 4K fallback");
    Check(dynamicShadowSampling.find("int PointShadowFace(vec3 ray)")
                            !=std::string::npos
                    &&dynamicShadowSampling.find(
                               "vec3 PointShadowFaceRay(int face, vec2 uv)")
                            !=std::string::npos
                    &&dynamicShadowSampling.find(
                               "sampleSlot = baseSlot + sampleFace")
                            !=std::string::npos
                    &&dynamicShadowSampling.find(
                               "ivec2 baseTexel = ivec2(floor(texelPosition))")
                            !=std::string::npos
                    &&dynamicShadowSampling.find("paraboloid")
                            ==std::string::npos,
          "surface point-shadow sampling reselects cube faces across seams and filters hard edges over four texels");
    Check(dynamicShadowSampling.find("bool rectProjection")
                            !=std::string::npos
                    &&dynamicShadowSampling.find("vec3 cubeFromLight")
                            !=std::string::npos
                    &&dynamicShadowSampling.find(
                               "frontHemisphereOnly && sampleFace == 5")
                            !=std::string::npos
                    &&dust.find("bool rectProjection")!=std::string::npos
                    &&dust.find("cubeFromLight")!=std::string::npos
                    &&dust.find("frontHemisphereOnly&&face==5")
                            !=std::string::npos,
          "surface and dust rect shadows sample a rect-local cube and reject the omitted back face");
    Check(ReadSource(SECTOR_SHADER_SOURCE_PATH).find(
                          "SECTOR_DYNAMIC_SURFACE_SHADOW_GLSL")
                            !=std::string::npos
                    &&ReadSource(DOOR_SHADER_SOURCE_PATH).find(
                               "SECTOR_DYNAMIC_SURFACE_SHADOW_GLSL")
                            !=std::string::npos
                    &&ReadSource(PBR_SHADER_SOURCE_PATH).find(
                               "SECTOR_DYNAMIC_SURFACE_SHADOW_GLSL")
                            !=std::string::npos
                    &&ReadSource(BILLBOARD_SHADER_SOURCE_PATH).find(
                               "SECTOR_DYNAMIC_SURFACE_SHADOW_GLSL")
                            !=std::string::npos
                    &&dust.find("pointShadowFace")!=std::string::npos,
          "all surface and volumetric receivers share cube-face shadow projection helpers");
    const std::size_t atlasReset=dynamicLightingShadows.find(
            "if (shadowAtlasNeedsFullClear) {");
    const std::size_t invalidateCachedTile=dynamicLightingShadows.find(
            "state.valid = false;",atlasReset);
    const std::size_t assignedTileFilter=dynamicLightingShadows.find(
            "if (!state.assigned) continue;",invalidateCachedTile);
    const std::size_t lifecycleFullClear=dynamicLightingShadows.find(
            "const bool fullClear = shadowAtlasNeedsFullClear;",
            assignedTileFilter);
    const std::size_t incrementalTileClear=dynamicLightingShadows.find(
            "if (!fullClear) glClear(GL_DEPTH_BUFFER_BIT);",
            lifecycleFullClear);
    Check(atlasReset!=std::string::npos
                    &&invalidateCachedTile!=std::string::npos
                    &&assignedTileFilter!=std::string::npos
                    &&lifecycleFullClear!=std::string::npos
                    &&incrementalTileClear!=std::string::npos
                    &&atlasReset<invalidateCachedTile
                    &&invalidateCachedTile<assignedTileFilter
                    &&assignedTileFilter<lifecycleFullClear
                    &&lifecycleFullClear<incrementalTileClear
                    &&dynamicLightingShadows.find(
                               "pendingShadowLightUpdates.size() == shadowCasters.size()")
                            ==std::string::npos,
          "full shadow-atlas clears invalidate every cached tile while routine updates clear only their scissored tiles");
    const std::string pbrModels=ReadSource(PBR_SHADER_SOURCE_PATH);
    Check(pbrModels.find("dynamicLightContext.shadowMaps.shadowMap0")
                            !=std::string::npos
                    &&pbrModels.find("dynamicModel.shadowMode")
                            ==std::string::npos,
          "dynamic models receive shared atlas shadows independently of their caster mode");
    Check(bloom.find("failedForCurrentKey")!=std::string::npos
                    && bloom.find("rlDrawRenderBatchActive")!=std::string::npos
                    && bloom.find("rlDisableColorBlend")!=std::string::npos
                    && bloom.find("rlEnableColorBlend")!=std::string::npos,
          "optional bloom failure is latched and rlgl blend transitions are synchronized/restored");
}

void TestFlashlightProfileCoverage()
{
    const std::string sector = ReadSource(SECTOR_SHADER_SOURCE_PATH);
    const std::string models = ReadSource(PBR_SHADER_SOURCE_PATH);
    const std::string doors = ReadSource(DOOR_SHADER_SOURCE_PATH);
    const std::string billboards = ReadSource(BILLBOARD_SHADER_SOURCE_PATH);
    const auto supportsFlashlight = [](const std::string& source) {
        return source.find("uniform sampler2D flashlightCookie")
                            != std::string::npos
                && source.find("FlashlightProfileFactor")
                            != std::string::npos
                && source.find("dynamicLightProfiles[i] == 1")
                            != std::string::npos;
    };
    Check(supportsFlashlight(sector)
                    && supportsFlashlight(models)
                    && supportsFlashlight(doors)
                    && supportsFlashlight(billboards),
          "all opaque and cutout dynamic-light receivers apply the projected flashlight profile");
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
    TestSectorRuntimeNormalMappingPolicy();
    TestBakedHdrConsumersStayUnclamped();
    TestDistanceFogUsesDarknessGatedScattering();
    TestHdrEffectShaderAndPassPolicies();
    TestFlashlightProfileCoverage();
    if (failures != 0) {
        std::fprintf(stderr, "%d PBR lighting policy test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
