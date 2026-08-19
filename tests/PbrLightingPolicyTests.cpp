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
}

std::string ReadSource(const char* path)
{
    std::ifstream input(path);
    return std::string(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
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
                      "staticAtmosphericLighting = max(fragColor.rgb + bakedDirect, vec3(0.0))")
                            != std::string::npos
                    && sector.find(
                               "ApplySectorFog(\n"
                               "            surfaceOutput,\n"
                               "            staticAtmosphericLighting,")
                            != std::string::npos,
          "sector fog uses ambient and baked direct light without baked AO or dynamic light");
    Check(door.find(
                      "ApplySectorFog(\n"
                      "            surfaceRgb * tint * lighting,\n"
                      "            staticProbeLighting,")
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
    const std::string fog=ReadSource(LOCAL_FOG_SHADER_SOURCE_PATH);
    const std::string haze=ReadSource(HAZE_SHADER_SOURCE_PATH);
    const std::string distanceFog=ReadSource(DISTANCE_FOG_SHADER_SOURCE_PATH);
    const std::string analyticFog=ReadSource(ANALYTIC_FOG_SHADER_SOURCE_PATH);
    const std::string lightProxy=ReadSource(LIGHT_PROXY_SHADER_SOURCE_PATH);
    const std::string dust=ReadSource(DUST_SHADER_SOURCE_PATH);
    const std::string muzzle=ReadSource(MUZZLE_SHADER_SOURCE_PATH);
    const std::string mainGraph=ReadSource(MAIN_RENDER_GRAPH_SOURCE_PATH);
    const std::string modelAssets=ReadSource(MODEL_ASSET_SOURCE_PATH);
    const std::string dynamicModelShadows=ReadSource(DYNAMIC_MODEL_SHADOW_SOURCE_PATH);
    const std::string dynamicLightingShadows=ReadSource(
            DYNAMIC_LIGHTING_SHADOW_SOURCE_PATH);
    Check(!bloom.empty()&&!fog.empty()&&!haze.empty()&&!distanceFog.empty()
                    &&!analyticFog.empty()&&!lightProxy.empty()&&!dust.empty()
                    &&!muzzle.empty()&&!mainGraph.empty()&&!dynamicModelShadows.empty()
                    &&!dynamicLightingShadows.empty(),
          "HDR effect policy can read every affected shader and pass graph");
    Check(distanceFog.find("for (") == std::string::npos
                    && analyticFog.find("for (int stepIndex") == std::string::npos
                    && lightProxy.find("for (int lightIndex") == std::string::npos
                    && distanceFog.find("uniform sampler2D sceneDepth") != std::string::npos
                    && analyticFog.find("intersectEllipsoid") != std::string::npos
                    && lightProxy.find("BeginBlendMode(BLEND_ADD_COLORS)") != std::string::npos,
          "cheap atmosphere paths use depth and analytic/proxy work without raymarch or light loops");
    Check(lightProxy.find("shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, \"sceneDepth\")")
                            != std::string::npos
                    && lightProxy.find("material.maps[MATERIAL_MAP_DIFFUSE].texture = sceneTarget.depth")
                            != std::string::npos
                    && lightProxy.find("SetShaderValueTexture(shader") == std::string::npos
                    && lightProxy.find("sourceVisibility") == std::string::npos
                    && lightProxy.find("fragSourceUv") == std::string::npos
                    && lightProxy.find("float depthDelta = proxyForward - sceneForward;")
                            != std::string::npos
                    && lightProxy.find("float occlusionBias = fragProxyData.x < 0.5 ? 0.12 : 0.03;")
                            != std::string::npos,
          "light proxies bind mesh depth deterministically and use local depth occlusion only");
    Check(bloom.find("Rgba8Unorm")==std::string::npos
                    && fog.find("Rgba8Unorm")==std::string::npos
                    && haze.find("Rgba8Unorm")==std::string::npos
                    && dust.find("Rgba8Unorm")==std::string::npos,
          "radiance-bearing atmosphere and bloom targets never select RGBA8");
    Check(fog.find("dynamicLightingClamp")==std::string::npos
                    && haze.find("dynamicLightingClamp")==std::string::npos
                    && dust.find("dynamicLightingClamp")==std::string::npos,
          "obsolete dynamic-light artistic ceilings stay removed from atmosphere");
    const std::size_t fogNoiseSetup = fog.find("float noiseModulation = 1.0;");
    const std::size_t fogNoiseSample = fog.find("valueNoise(", fogNoiseSetup);
    const std::size_t fogMarch = fog.find("for (int stepIndex", fogNoiseSetup);
    const std::size_t hazeLightingMarch = haze.find("for(int s=0;s<12");
    const std::size_t hazeSecondMarch = haze.find("for(int s=0;s<12", hazeLightingMarch + 1);
    const std::size_t hazeNoiseSample = haze.find("valueNoise(", hazeLightingMarch);
    const std::size_t hazeSampleDepth = haze.find(
            "sampleDepth=a.x*boundary*noiseModulation*opticalStep",
            hazeLightingMarch);
    Check(fogNoiseSetup!=std::string::npos
                    && fogNoiseSample<fogMarch
                    && fog.find("noiseSamplePosition.xz -= flowWorld;")!=std::string::npos
                    && fog.find("modulatedOpticalDepth = volumeOpticalDepth * noiseModulation")
                            !=std::string::npos
                    && hazeLightingMarch!=std::string::npos
                    && hazeSecondMarch==std::string::npos
                    && hazeSampleDepth!=std::string::npos
                    && hazeNoiseSample>hazeLightingMarch
                    && hazeNoiseSample<hazeSampleDepth
                    && haze.find("noiseSamplePosition.xz-=flowWorld;")!=std::string::npos
                    && haze.find("insidePositionSum")==std::string::npos
                    && haze.find("modulatedDepth=volumeDepth*noiseModulation")==std::string::npos,
          "fog uses coherent ray noise while haze samples world-speed noise at each lighting step");
    Check(haze.find("bool intersectFiniteCone(")!=std::string::npos
                    && haze.find("if(hazeShapes[i]!=0) { float coneEnter,coneExit;")!=std::string::npos
                    && haze.find("effectivePath(segment,thickness)/float(stepCount)")!=std::string::npos
                    && haze.find("insideCount")==std::string::npos,
          "haze marches once across exact sphere or finite-cone ray intervals");
    Check(haze.find("volume.originWorld.y -= heightOffsetWorld")!=std::string::npos
                    && haze.find("volume.boundsCenterWorld.y -= heightOffsetWorld")!=std::string::npos
                    && haze.find("hazeOwnerDynamicLightIndices[volumeIndex]")!=std::string::npos
                    && haze.find("?p-vec3(0,heightOffsetWorld,0):p")!=std::string::npos
                    && haze.find("shadowVisibility(i,slot,lightingPosition)")!=std::string::npos
                    && haze.find("dynamicLighting(p,i,b.w)")!=std::string::npos
                    && haze.find("h.flowSpeedWorld,h.heightOffsetWorld")!=std::string::npos,
          "haze translates its baked and owning dynamic-light illumination profile with its Y offset");
    Check(bloom.find("65504.0")!=std::string::npos
                    && fog.find("65504.0")!=std::string::npos
                    && haze.find("65504.0")!=std::string::npos
                    && dust.find("65504.0")!=std::string::npos
                    && muzzle.find("65504.0")!=std::string::npos,
          "affected RGBA16F writes retain the named finite-half storage guard");
    Check(bloom.find("finalColor = vec4(SanitizeLinearHdrForRgba16f(color), 0.0)")
                    !=std::string::npos
                    && bloom.find("SafeAlpha(scene.a)")!=std::string::npos,
          "bloom buffers ignore alpha energy and composition preserves scene alpha");
    Check(fog.find("* (1.0 - SanitizeOpacity(fog.a))")!=std::string::npos
                    && fog.find("+ SanitizeIntermediateRadiance(fog.rgb)")!=std::string::npos
                    && haze.find("*(1.0-SanitizeOpacity(haze.a))")!=std::string::npos
                    && haze.find("+SanitizeIntermediateRadiance(haze.rgb)")!=std::string::npos,
          "fog and haze use sanitized premultiplied scattering plus transmittance");
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
                    && fog.find("LinearToSrgb")==std::string::npos
                    && haze.find("LinearToSrgb")==std::string::npos
                    && dust.find("LinearToSrgb")==std::string::npos
                    && muzzle.find("LinearToSrgb")==std::string::npos
                    && bloom.find("ToneMap")==std::string::npos,
          "effect shaders do not tone map or output-encode radiance locally");
    Check(fog.find("uniform uint fogDynamicLightMasks[16]")!=std::string::npos
                    &&fog.find("fogDynamicLightMasks[volumeIndex]")!=std::string::npos
                    &&haze.find("uniform uint hazeDynamicLightMasks[8]")!=std::string::npos
                    &&haze.find("hazeDynamicLightMasks[volumeIndex]")!=std::string::npos,
          "volumetric shaders reject dynamically lit samples outside conservative volume masks");
    Check(fog.find("scale = 0.5f")!=std::string::npos
                    &&fog.find("steps = 8")!=std::string::npos
                    &&fog.find("cap = 8")!=std::string::npos
                    &&haze.find("int cap=4, steps=8; float renderScale=0.5f")
                            !=std::string::npos,
          "lossless atmosphere culling preserves medium volumetric quality settings");
    Check(bloom.find("ApplyEmissiveDecalBloom")==std::string::npos
                    && ReadSource(SECTOR_SHADER_SOURCE_PATH).find(
                               "emissiveRadiance * emissiveDecalAlpha")!=std::string::npos
                    && ReadSource(SECTOR_SHADER_SOURCE_PATH).find(
                               "surfaceRgb = mix(baseColor.rgb, decalRgb, decalAlpha)")
                            !=std::string::npos,
          "decal-only bloom redraw is retired in favor of visible emissive radiance");
    const std::size_t atmosphere=mainGraph.find("Apply3DWorldAtmosphere");
    const std::size_t viewmodel=mainGraph.find("Render3DViewmodel");
    const std::size_t sceneBloom=mainGraph.find("Apply3DHdrBloom");
    const std::size_t overlays=mainGraph.find("Render3DOverlays");
    Check(atmosphere<viewmodel&&viewmodel<sceneBloom&&sceneBloom<overlays,
          "pass graph orders atmosphere, viewmodel, bloom, then excluded editor overlays");
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
    Check(sectorRenderer.find("map.fogSettings.localVolumeQuality")
                            ==std::string::npos
                    &&sectorRenderer.find(
                               "map,\n            volumetricQuality,")
                            !=std::string::npos,
          "application volumetric quality directly controls local fog and light haze");
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
    const std::size_t secondSkinnedShadowVertex=dynamicLightingShadows.find(
            "in vec4 vertexBoneIndices;",firstSkinnedShadowVertex+1);
    Check(firstSkinnedShadowVertex!=std::string::npos
                    &&secondSkinnedShadowVertex!=std::string::npos
                    &&dynamicLightingShadows.find("BuildAnimatedModelPoseView")
                            !=std::string::npos
                    &&dynamicLightingShadows.find("dynamicModelShadowCasters")
                            !=std::string::npos
                    &&dynamicLightingShadows.find("contentFingerprint")
                            !=std::string::npos,
          "shared spotlight and point-light atlas passes render and invalidate posed dynamic-model casters");
    const std::size_t geometryShaderLoader=dynamicLightingShadows.find(
            "Shader LoadGeometryShader");
    const std::size_t pointBoneIndexBinding=dynamicLightingShadows.find(
            "RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES",
            geometryShaderLoader);
    const std::size_t pointBoneWeightBinding=dynamicLightingShadows.find(
            "RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS",
            geometryShaderLoader);
    const std::size_t geometryShaderLink=dynamicLightingShadows.find(
            "glLinkProgram(program)",geometryShaderLoader);
    Check(geometryShaderLoader!=std::string::npos
                    &&pointBoneIndexBinding!=std::string::npos
                    &&pointBoneWeightBinding!=std::string::npos
                    &&geometryShaderLink!=std::string::npos
                    &&pointBoneIndexBinding<geometryShaderLink
                    &&pointBoneWeightBinding<geometryShaderLink,
          "point-light geometry shader binds raylib bone VAO attributes before linking");
    const std::size_t pointTrianglePayload=dynamicLightingShadows.find(
            "flat out vec3 fragTriangleOrigin;");
    const std::size_t pointShadowVertexBudget=dynamicLightingShadows.find(
            "layout(triangle_strip, max_vertices = 52) out;");
    const std::size_t pointTriangleContainment=dynamicLightingShadows.find(
            "const float triangleContainmentTolerance = 0.0001;");
    const std::size_t pointTriangleDiscard=dynamicLightingShadows.find(
            "barycentricU + barycentricV\n"
            "                    > 1.0 + triangleContainmentTolerance) discard;");
    const std::size_t pointShadowDepthWrite=dynamicLightingShadows.find(
            "gl_FragDepth = clamp(distanceToLight / pointLightRadius",
            pointTriangleContainment);
    Check(pointShadowVertexBudget!=std::string::npos
                    &&pointTrianglePayload!=std::string::npos
                    &&dynamicLightingShadows.find(
                               "flat out vec3 fragTriangleEdge0;")
                            !=std::string::npos
                    &&dynamicLightingShadows.find(
                               "flat out vec3 fragTriangleEdge1;")
                            !=std::string::npos
                    &&dynamicLightingShadows.find("fragTrianglePlane")
                            ==std::string::npos
                    &&pointTriangleContainment!=std::string::npos
                    &&pointTriangleDiscard!=std::string::npos
                    &&pointShadowDepthWrite!=std::string::npos
                    &&pointTriangleContainment<pointTriangleDiscard
                    &&pointTriangleDiscard<pointShadowDepthWrite,
          "point-light shadow casters stay within the geometry output budget and reject paraboloid chord overdraw");
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

} // namespace

int main()
{
    TestDiagnosticModesAndScales();
    TestIndirectAndEnvironmentRouting();
    TestViewmodelIsolationAndFiniteHandling();
    TestMaterialTextureSemantics();
    TestEnvironmentEligibility();
    TestRemovedShaderPathsStayRemoved();
    TestBakedHdrConsumersStayUnclamped();
    TestDistanceFogUsesDarknessGatedScattering();
    TestHdrEffectShaderAndPassPolicies();
    if (failures != 0) {
        std::fprintf(stderr, "%d PBR lighting policy test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
