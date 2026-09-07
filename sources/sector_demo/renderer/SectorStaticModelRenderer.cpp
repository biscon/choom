#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"
#include "sector_demo/renderer/SectorReflectionProbePolicy.h"

#include "sector_demo/renderer/SectorAtmosphereCulling.h"

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "engine/components/AnimatedModel.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace game {
namespace {

int GetShaderLocationArrayBase(Shader shader, const char* name)
{
    const int location = GetShaderLocation(shader, name);
    if (location >= 0) {
        return location;
    }
    const std::string indexedName = std::string(name) + "[0]";
    return GetShaderLocation(shader, indexedName.c_str());
}

int GetShaderLocationArrayElement(Shader shader, const char* name, size_t index)
{
    const std::string indexedName = std::string(name)
            + "[" + std::to_string(index) + "]";
    return GetShaderLocation(shader, indexedName.c_str());
}

void SetShaderSamplerUnit(Shader shader, int location, int textureUnit)
{
    if (location >= 0) {
        SetShaderValue(
                shader,
                location,
                &textureUnit,
                SHADER_UNIFORM_INT);
    }
}

void InitializeSectorPbrSamplerUnits(
        Shader shader,
        int lightmapTextureLocation,
        int environmentTextureLocation,
        int shadowMap0Location,
        int shadowMap1Location)
{
    constexpr std::array<int, 6> materialTextureUnits{
            MATERIAL_MAP_ALBEDO,
            MATERIAL_MAP_METALNESS,
            MATERIAL_MAP_NORMAL,
            MATERIAL_MAP_ROUGHNESS,
            MATERIAL_MAP_OCCLUSION,
            MATERIAL_MAP_EMISSION};
    for (int textureUnit : materialTextureUnits) {
        SetShaderSamplerUnit(
                shader,
                shader.locs[SHADER_LOC_MAP_DIFFUSE + textureUnit],
                textureUnit);
    }
    SetShaderSamplerUnit(
            shader,
            lightmapTextureLocation,
            SectorStaticModelLightmapMaterialMap);
    // A missing environment leaves this sampler active in the linked shader.
    // Keep its cubemap unit distinct from the default sampler2D unit even when
    // DrawMesh() has no cubemap texture to bind for the current map.
    SetShaderSamplerUnit(
            shader,
            environmentTextureLocation,
            SectorStaticModelEnvironmentMaterialMap);
    SetShaderSamplerUnit(
            shader,
            shadowMap0Location,
            SectorStaticModelShadowMap0MaterialMap);
    SetShaderSamplerUnit(
            shader,
            shadowMap1Location,
            SectorStaticModelShadowMap1MaterialMap);
    rlDisableShader();
}

void AppendStaticModelDebugText(
        std::string& renderDebugText,
        size_t drawn,
        size_t considered,
        size_t portalCulled,
        size_t skipped)
{
    const size_t existing = renderDebugText.find(" | props:");
    if (existing != std::string::npos) {
        renderDebugText.erase(existing);
    }
    if (!renderDebugText.empty()) {
        renderDebugText += " | props: "
                + std::to_string(drawn)
                + " drawn / "
                + std::to_string(considered)
                + " considered, "
                + std::to_string(portalCulled)
                + " portal culled, "
                + std::to_string(skipped)
                + " skipped";
    }
}

Vector4 ColorToNormalizedVector4(Color color)
{
    constexpr float scale = 1.0f / 255.0f;
    return Vector4{
            static_cast<float>(color.r) * scale,
            static_cast<float>(color.g) * scale,
            static_cast<float>(color.b) * scale,
            static_cast<float>(color.a) * scale};
}

const SectorStaticModelLightmapObject* FindLightmapObject(
        const SectorStaticModelLightmapData& data,
        int objectId)
{
    const auto found = std::lower_bound(
            data.objects.begin(),
            data.objects.end(),
            objectId,
            [](const SectorStaticModelLightmapObject& object, int id) {
                return object.objectId < id;
            });
    return found != data.objects.end() && found->objectId == objectId
            ? &*found
            : nullptr;
}

bool BuildRemappedMesh(
        const Mesh& source,
        const SectorStaticModelLightmapMesh& remap,
        Mesh& outMesh)
{
    outMesh = {};
    if (source.vertexCount != remap.originalVertexCount
            || source.vertices == nullptr
            || remap.indices.empty()
            || remap.indices.size()
                    > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    outMesh.vertexCount = static_cast<int>(remap.indices.size());
    outMesh.triangleCount = outMesh.vertexCount / 3;
    outMesh.vertices = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 3 * sizeof(float))));
    outMesh.normals = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 3 * sizeof(float))));
    outMesh.texcoords = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 2 * sizeof(float))));
    outMesh.texcoords2 = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 2 * sizeof(float))));
    outMesh.colors = static_cast<unsigned char*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 4 * sizeof(unsigned char))));
    if (source.tangents != nullptr) {
        outMesh.tangents = static_cast<float*>(
                MemAlloc(static_cast<unsigned int>(
                        static_cast<size_t>(outMesh.vertexCount)
                        * 4 * sizeof(float))));
    }
    if (outMesh.vertices == nullptr
            || outMesh.normals == nullptr
            || outMesh.texcoords == nullptr
            || outMesh.texcoords2 == nullptr
            || outMesh.colors == nullptr
            || (source.tangents != nullptr && outMesh.tangents == nullptr)) {
        UnloadMesh(outMesh);
        outMesh = {};
        return false;
    }

    for (int vertexIndex = 0;
            vertexIndex < outMesh.vertexCount;
            ++vertexIndex) {
        const uint32_t remapIndex =
                remap.indices[static_cast<size_t>(vertexIndex)];
        if (remapIndex >= remap.sourceVertexIndices.size()
                || remapIndex >= remap.localLightmapUvs.size()) {
            UnloadMesh(outMesh);
            outMesh = {};
            return false;
        }
        const uint32_t sourceIndex =
                remap.sourceVertexIndices[remapIndex];
        if (sourceIndex >= static_cast<uint32_t>(source.vertexCount)) {
            UnloadMesh(outMesh);
            outMesh = {};
            return false;
        }
        std::copy_n(
                source.vertices + sourceIndex * 3,
                3,
                outMesh.vertices + vertexIndex * 3);
        if (source.normals != nullptr) {
            std::copy_n(
                    source.normals + sourceIndex * 3,
                    3,
                    outMesh.normals + vertexIndex * 3);
        } else {
            std::fill_n(outMesh.normals + vertexIndex * 3, 3, 0.0f);
        }
        if (source.texcoords != nullptr) {
            std::copy_n(
                    source.texcoords + sourceIndex * 2,
                    2,
                    outMesh.texcoords + vertexIndex * 2);
        } else {
            std::fill_n(outMesh.texcoords + vertexIndex * 2, 2, 0.0f);
        }
        const Vector2 lightmapUv =
                remap.localLightmapUvs[remapIndex];
        outMesh.texcoords2[vertexIndex * 2] = lightmapUv.x;
        outMesh.texcoords2[vertexIndex * 2 + 1] = lightmapUv.y;
        if (source.colors != nullptr) {
            std::copy_n(
                    source.colors + sourceIndex * 4,
                    4,
                    outMesh.colors + vertexIndex * 4);
        } else {
            std::fill_n(
                    outMesh.colors + vertexIndex * 4,
                    4,
                    static_cast<unsigned char>(255));
        }
        if (source.tangents != nullptr) {
            std::copy_n(
                    source.tangents + sourceIndex * 4,
                    4,
                    outMesh.tangents + vertexIndex * 4);
        }
    }
    for (int triangle = 0;
            triangle < outMesh.triangleCount;
            ++triangle) {
        float* normal0 = outMesh.normals + (triangle * 3) * 3;
        float* normal1 = outMesh.normals + (triangle * 3 + 1) * 3;
        float* normal2 = outMesh.normals + (triangle * 3 + 2) * 3;
        const Vector3 n0{normal0[0], normal0[1], normal0[2]};
        const Vector3 n1{normal1[0], normal1[1], normal1[2]};
        const Vector3 n2{normal2[0], normal2[1], normal2[2]};
        if (Vector3LengthSqr(n0) > 0.0000001f
                && Vector3LengthSqr(n1) > 0.0000001f
                && Vector3LengthSqr(n2) > 0.0000001f) {
            continue;
        }
        const float* vertex0 =
                outMesh.vertices + (triangle * 3) * 3;
        const float* vertex1 =
                outMesh.vertices + (triangle * 3 + 1) * 3;
        const float* vertex2 =
                outMesh.vertices + (triangle * 3 + 2) * 3;
        const Vector3 faceNormal = Vector3Normalize(Vector3CrossProduct(
                Vector3Subtract(
                        Vector3{vertex1[0], vertex1[1], vertex1[2]},
                        Vector3{vertex0[0], vertex0[1], vertex0[2]}),
                Vector3Subtract(
                        Vector3{vertex2[0], vertex2[1], vertex2[2]},
                        Vector3{vertex0[0], vertex0[1], vertex0[2]})));
        for (int corner = 0; corner < 3; ++corner) {
            float* normal =
                    outMesh.normals + (triangle * 3 + corner) * 3;
            normal[0] = faceNormal.x;
            normal[1] = faceNormal.y;
            normal[2] = faceNormal.z;
        }
    }
    UploadMesh(&outMesh, false);
    return outMesh.vaoId != 0;
}

} // namespace

const char* SectorPbrDiagnosticModeName(SectorPbrDiagnosticMode mode)
{
    switch (mode) {
        case SectorPbrDiagnosticMode::Full: return "Full PBR";
        case SectorPbrDiagnosticMode::BaseColor: return "Base Color";
        case SectorPbrDiagnosticMode::DirectDiffuse: return "Direct Diffuse";
        case SectorPbrDiagnosticMode::DirectSpecular: return "Direct Specular";
        case SectorPbrDiagnosticMode::IndirectDiffuse: return "Probe / Indirect Diffuse";
        case SectorPbrDiagnosticMode::EnvironmentSpecular: return "Environment Specular";
        case SectorPbrDiagnosticMode::Emissive: return "Emissive";
        case SectorPbrDiagnosticMode::MaterialOcclusion: return "Material AO";
        case SectorPbrDiagnosticMode::MetallicRoughness: return "Metallic / Roughness";
        case SectorPbrDiagnosticMode::ShadingNormal: return "Shading Normal";
        case SectorPbrDiagnosticMode::TangentNormal: return "Tangent-Space Normal";
        case SectorPbrDiagnosticMode::Count: break;
    }
    return "Full PBR";
}

const char* SectorPbrLightingPathName(SectorPbrLightingPath path)
{
    switch (path) {
        case SectorPbrLightingPath::WorldStatic: return "world static model";
        case SectorPbrLightingPath::WorldDynamic: return "world dynamic model";
        case SectorPbrLightingPath::Viewmodel: return "viewmodel arms";
        case SectorPbrLightingPath::ViewmodelAttachment: return "viewmodel attachment";
    }
    return "unknown";
}

const char* SectorPbrIndirectSourceName(SectorPbrIndirectSource source)
{
    switch (source) {
        case SectorPbrIndirectSource::SectorAmbient: return "sector ambient fallback";
        case SectorPbrIndirectSource::ObjectProbe: return "object probe";
        case SectorPbrIndirectSource::StaticLightmap: return "static model lightmap";
    }
    return "unknown";
}

bool SectorStaticModelRenderer::Load()
{
    shader = LoadGameShader(GameShader::StaticModel);
    if (shader.id == 0) {
        shader = {};
        shaderLoaded = false;
        return false;
    }
    reflectionLocations = LoadSectorReflectionShaderLocations(shader);

    shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] =
            GetShaderLocationAttrib(shader, "vertexNormal");
    shader.locs[SHADER_LOC_VERTEX_TANGENT] =
            GetShaderLocationAttrib(shader, "vertexTangent");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] =
            GetShaderLocationAttrib(shader, "vertexTexCoord2");
    shader.locs[SHADER_LOC_VERTEX_COLOR] =
            GetShaderLocationAttrib(shader, "vertexColor");
    shader.locs[SHADER_LOC_VERTEX_BONEIDS] =
            GetShaderLocationAttrib(shader, "vertexBoneIndices");
    shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] =
            GetShaderLocationAttrib(shader, "vertexBoneWeights");
    shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] =
            GetShaderLocation(shader, "boneMatrices");
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "baseColorTexture");
    shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(shader, "metallicTexture");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "normalTexture");
    shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(shader, "roughnessTexture");
    shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(shader, "occlusionTexture");
    shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(shader, "emissiveTexture");

    baseColorFactorLoc = GetShaderLocation(shader, "baseColorFactor");
    emissiveFactorLoc = GetShaderLocation(shader, "emissiveFactor");
    emissiveStrengthLoc = GetShaderLocation(shader, "emissiveStrength");
    metallicFactorLoc = GetShaderLocation(shader, "metallicFactor");
    roughnessFactorLoc = GetShaderLocation(shader, "roughnessFactor");
    normalScaleLoc = GetShaderLocation(shader, "normalScale");
    occlusionStrengthLoc = GetShaderLocation(shader, "occlusionStrength");
    modelOpacityLoc = GetShaderLocation(shader, "modelOpacity");
    interactionHighlightStrengthLoc = GetShaderLocation(
            shader, "interactionHighlightStrength");
    hasBaseColorTextureLoc = GetShaderLocation(shader, "hasBaseColorTexture");
    hasMetallicTextureLoc = GetShaderLocation(shader, "hasMetallicTexture");
    hasNormalTextureLoc = GetShaderLocation(shader, "hasNormalTexture");
    hasRoughnessTextureLoc = GetShaderLocation(shader, "hasRoughnessTexture");
    hasOcclusionTextureLoc = GetShaderLocation(shader, "hasOcclusionTexture");
    hasEmissiveTextureLoc = GetShaderLocation(shader, "hasEmissiveTexture");
    baseColorHardwareSrgbLoc = GetShaderLocation(
            shader, "baseColorTextureHardwareSrgb");
    emissiveHardwareSrgbLoc = GetShaderLocation(
            shader, "emissiveTextureHardwareSrgb");
    diagnosticModeLoc = GetShaderLocation(shader, "pbrDiagnosticMode");
    indirectDiffuseScaleLoc = GetShaderLocation(
            shader, "indirectDiffuseScale");
    environmentSpecularScaleLoc = GetShaderLocation(
            shader, "environmentSpecularScale");
    environmentBoxProjectionLoc = GetShaderLocation(shader, "environmentBoxProjection");
    environmentCapturePositionLoc = GetShaderLocation(shader, "environmentCapturePosition");
    environmentInfluenceCenterLoc = GetShaderLocation(shader, "environmentInfluenceCenter");
    environmentHalfExtentsLoc = GetShaderLocation(shader, "environmentHalfExtents");
    environmentYawLoc = GetShaderLocation(shader, "environmentYaw");
    environmentMaxLodLoc = GetShaderLocation(shader, "environmentMaxLod");
    environmentIntensityLoc = GetShaderLocation(shader, "environmentIntensity");
    useStaticSpecularLightingLoc = GetShaderLocation(
            shader, "useStaticSpecularLighting");
    cameraPositionLoc = GetShaderLocation(shader, "cameraPosition");
    environmentExposureLoc = GetShaderLocation(shader, "environmentExposure");
    outputBrightnessMultiplierLoc =
            GetShaderLocation(shader, "outputBrightnessMultiplier");
    hasEnvironmentLoc = GetShaderLocation(shader, "hasEnvironment");
    environmentTextureLoc = GetShaderLocation(shader, "environmentTexture");
    lightmapScaleBiasLoc =
            GetShaderLocation(shader, "lightmapScaleBias");
    hasStaticLightmapLoc =
            GetShaderLocation(shader, "hasStaticLightmap");
    useBakedAmbientOcclusionLoc =
            GetShaderLocation(shader, "useBakedAmbientOcclusion");
    containingSectorAmbientLoc =
            GetShaderLocation(shader, "containingSectorAmbient");
    useObjectProbeLightingLoc =
            GetShaderLocation(shader, "useObjectProbeLighting");
    for (size_t i = 0; i < objectAmbientCubeLocs.size(); ++i) {
        objectAmbientCubeLocs[i] = GetShaderLocationArrayElement(
                shader, "objectAmbientCube", i);
        objectAmbientCubeUpperLocs[i] = GetShaderLocationArrayElement(
                shader, "objectAmbientCubeUpper", i);
    }
    objectAmbientCubeLowerHeightLoc =
            GetShaderLocation(shader, "objectAmbientCubeLowerHeight");
    objectAmbientCubeUpperHeightLoc =
            GetShaderLocation(shader, "objectAmbientCubeUpperHeight");
    useVerticalObjectProbeLightingLoc =
            GetShaderLocation(shader, "useVerticalObjectProbeLighting");
    useSkinningLoc = GetShaderLocation(shader, "useSkinning");
    lightmapTextureLoc =
            GetShaderLocation(shader, "lightmapTexture");
    shader.locs[
            SHADER_LOC_MAP_DIFFUSE
                    + SectorStaticModelLightmapMaterialMap] =
            lightmapTextureLoc;
    shader.locs[
            SHADER_LOC_MAP_DIFFUSE
                    + SectorStaticModelEnvironmentMaterialMap] =
            environmentTextureLoc;
    dynamicLightCountLoc = GetShaderLocation(shader, "dynamicLightCount");
    dynamicLightPositionsLoc = GetShaderLocationArrayBase(shader, "dynamicLightPositions");
    dynamicLightColorsLoc = GetShaderLocationArrayBase(shader, "dynamicLightColors");
    dynamicLightRadiiLoc = GetShaderLocationArrayBase(shader, "dynamicLightRadii");
    dynamicLightIntensitiesLoc = GetShaderLocationArrayBase(shader, "dynamicLightIntensities");
    dynamicLightTypesLoc = GetShaderLocationArrayBase(shader, "dynamicLightTypes");
    dynamicLightDirectionsLoc = GetShaderLocationArrayBase(shader, "dynamicLightDirections");
    dynamicLightInnerConeCosLoc = GetShaderLocationArrayBase(shader, "dynamicLightInnerConeCos");
    dynamicLightOuterConeCosLoc = GetShaderLocationArrayBase(shader, "dynamicLightOuterConeCos");
    dynamicLightSpotShadowRightLoc = GetShaderLocationArrayBase(
            shader, "dynamicLightSpotShadowRight");
    dynamicLightSpotShadowProjectionLoc = GetShaderLocationArrayBase(
            shader, "dynamicLightSpotShadowProjection");
    dynamicLightProfilesLoc = GetShaderLocationArrayBase(
            shader, "dynamicLightProfiles");
    dynamicLightProfileParametersLoc = GetShaderLocationArrayBase(
            shader, "dynamicLightProfileParameters");
    flashlightCookieLoc = GetShaderLocation(shader, "flashlightCookie");
    hasPointShadowsLoc = GetShaderLocation(shader, "hasPointShadows");
    staticSpecularLocations = GetSectorStaticSpecularShaderLocations(shader);
    dynamicLightShadowSlotsLoc = GetShaderLocationArrayBase(shader, "dynamicLightShadowSlots");
    for (size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        shadowLightMatrixLocs[i] =
                GetShaderLocationArrayElement(shader, "shadowLightMatrices", i);
    }
    shadowBiasLoc = GetShaderLocationArrayBase(shader, "shadowBias");
    shadowStrengthLoc = GetShaderLocationArrayBase(shader, "shadowStrength");
    shadowSoftnessLoc = GetShaderLocationArrayBase(shader, "shadowSoftness");
    shadowAtlasTilesPerRowLoc = GetShaderLocation(shader, "shadowAtlasTilesPerRow");
    shadowMap0Loc = GetShaderLocation(shader, "shadowMap0");
    shadowMap1Loc = GetShaderLocation(shader, "shadowMap1");
    shader.locs[
            SHADER_LOC_MAP_DIFFUSE
                    + SectorStaticModelShadowMap0MaterialMap] =
            shadowMap0Loc;
    shader.locs[
            SHADER_LOC_MAP_DIFFUSE
                    + SectorStaticModelShadowMap1MaterialMap] =
            shadowMap1Loc;
    InitializeSectorPbrSamplerUnits(
            shader,
            lightmapTextureLoc,
            environmentTextureLoc,
            shadowMap0Loc,
            shadowMap1Loc);
    fogShaderLocations = GetSectorFogShaderLocations(shader);
    shaderLoaded = true;
    return true;
}

void SectorStaticModelRenderer::Shutdown()
{
    staticDraws.clear();
    modelDoorDraws.clear();
    drawingModelDoor = false;
    ClearCachedModels();
    shadowCasterCollection = {};
    lightmapData = {};
    if (shaderLoaded) {
        UnloadShader(shader);
    }
    shader = {};
    baseColorFactorLoc = -1;
    emissiveFactorLoc = -1;
    emissiveStrengthLoc = -1;
    metallicFactorLoc = -1;
    roughnessFactorLoc = -1;
    normalScaleLoc = -1;
    occlusionStrengthLoc = -1;
    modelOpacityLoc = -1;
    interactionHighlightStrengthLoc = -1;
    hasBaseColorTextureLoc = -1;
    hasMetallicTextureLoc = -1;
    hasNormalTextureLoc = -1;
    hasRoughnessTextureLoc = -1;
    hasOcclusionTextureLoc = -1;
    hasEmissiveTextureLoc = -1;
    baseColorHardwareSrgbLoc = -1;
    emissiveHardwareSrgbLoc = -1;
    diagnosticModeLoc = -1;
    indirectDiffuseScaleLoc = -1;
    environmentSpecularScaleLoc = -1;
    environmentBoxProjectionLoc = -1;
    environmentCapturePositionLoc = -1;
    environmentInfluenceCenterLoc = -1;
    environmentHalfExtentsLoc = -1;
    environmentYawLoc = -1;
    environmentMaxLodLoc = -1;
    environmentIntensityLoc = -1;
    useStaticSpecularLightingLoc = -1;
    cameraPositionLoc = -1;
    environmentExposureLoc = -1;
    outputBrightnessMultiplierLoc = -1;
    hasEnvironmentLoc = -1;
    environmentTextureLoc = -1;
    lightmapScaleBiasLoc = -1;
    hasStaticLightmapLoc = -1;
    useBakedAmbientOcclusionLoc = -1;
    containingSectorAmbientLoc = -1;
    useObjectProbeLightingLoc = -1;
    objectAmbientCubeLocs.fill(-1);
    objectAmbientCubeUpperLocs.fill(-1);
    objectAmbientCubeLowerHeightLoc = -1;
    objectAmbientCubeUpperHeightLoc = -1;
    useVerticalObjectProbeLightingLoc = -1;
    useSkinningLoc = -1;
    lightmapTextureLoc = -1;
    dynamicLightCountLoc = -1;
    dynamicLightPositionsLoc = -1;
    dynamicLightColorsLoc = -1;
    dynamicLightRadiiLoc = -1;
    dynamicLightIntensitiesLoc = -1;
    dynamicLightTypesLoc = -1;
    dynamicLightDirectionsLoc = -1;
    dynamicLightInnerConeCosLoc = -1;
    dynamicLightOuterConeCosLoc = -1;
    dynamicLightSpotShadowRightLoc = -1;
    dynamicLightSpotShadowProjectionLoc = -1;
    dynamicLightProfilesLoc = -1;
    dynamicLightProfileParametersLoc = -1;
    flashlightCookieLoc = -1;
    hasPointShadowsLoc = -1;
    staticSpecularLocations = SectorStaticSpecularShaderLocations{};
    dynamicLightShadowSlotsLoc = -1;
    shadowLightMatrixLocs.fill(-1);
    shadowBiasLoc = -1;
    shadowStrengthLoc = -1;
    shadowSoftnessLoc = -1;
    shadowMap0Loc = -1;
    shadowMap1Loc = -1;
    fogShaderLocations = SectorFogShaderLocations{};
    shaderLoaded = false;
    warningPrinted = false;
}

void SectorStaticModelRenderer::ResetDebugState()
{
    warningPrinted = false;
    worldDiagnostics = {};
    viewmodelDiagnostics = {};
}

void SectorStaticModelRenderer::PrepareReceiverEnvironment(Vector3 position, int sector, const SectorReceiverBounds* bounds)
{
    if (!reflectionEnvironment) return;
    reflectionReceiverPosition=position;
    const BoundingBox box = bounds ? BoundingBox{bounds->min,bounds->max} : BoundingBox{position,position};
    environmentBlend=SelectSectorPbrEnvironmentBlend(*reflectionEnvironment,position,sector,true,&box,
            reflectionEnvironment->demandCollector);
    environmentSelection=environmentBlend.first;
}

void SectorStaticModelRenderer::UploadPbrDrawState(
        const SectorPbrDrawState& state)
{
    const int diagnosticMode = contributionSettings.reflectionCapture ? 11 : static_cast<int>(state.diagnosticMode);
    if (drawAssets) {
        auto blend = contributionSettings.reflectionCapture ? SectorPbrEnvironmentBlend{} : environmentBlend;
        UploadSectorReflectionBlend(shader,reflectionLocations,blend,*drawAssets,state.environmentExposure);
    }
    const int useObjectProbe = state.useObjectProbe ? 1 : 0;
    const int useVerticalProbe = state.useVerticalObjectProbe ? 1 : 0;
    const int hasEnvironment = state.environmentActive ? 1 : 0;
    const int useStaticSpecular = state.staticSpecularEligible ? 1 : 0;
    if (diagnosticModeLoc >= 0) SetShaderValue(shader, diagnosticModeLoc, &diagnosticMode, SHADER_UNIFORM_INT);
    if (indirectDiffuseScaleLoc >= 0) SetShaderValue(shader, indirectDiffuseScaleLoc, &state.indirectDiffuseScale, SHADER_UNIFORM_FLOAT);
    if (environmentSpecularScaleLoc >= 0) SetShaderValue(shader, environmentSpecularScaleLoc, &state.environmentSpecularScale, SHADER_UNIFORM_FLOAT);
    if (useStaticSpecularLightingLoc >= 0) SetShaderValue(shader, useStaticSpecularLightingLoc, &useStaticSpecular, SHADER_UNIFORM_INT);
    const float environmentExposure = environmentSelection.localProbe
            ? 1.0f : state.environmentExposure;
    if (environmentExposureLoc >= 0) SetShaderValue(shader, environmentExposureLoc, &environmentExposure, SHADER_UNIFORM_FLOAT);
    if (outputBrightnessMultiplierLoc >= 0) SetShaderValue(shader, outputBrightnessMultiplierLoc, &state.outputBrightnessMultiplier, SHADER_UNIFORM_FLOAT);
    if (hasEnvironmentLoc >= 0) SetShaderValue(shader, hasEnvironmentLoc, &hasEnvironment, SHADER_UNIFORM_INT);
    const int boxProjection = environmentSelection.boxProjection ? 1 : 0;
    if (environmentBoxProjectionLoc >= 0) SetShaderValue(shader, environmentBoxProjectionLoc, &boxProjection, SHADER_UNIFORM_INT);
    if (environmentCapturePositionLoc >= 0) SetShaderValue(shader, environmentCapturePositionLoc, &environmentSelection.capturePosition, SHADER_UNIFORM_VEC3);
    if (environmentInfluenceCenterLoc >= 0) SetShaderValue(shader, environmentInfluenceCenterLoc, &environmentSelection.influenceCenter, SHADER_UNIFORM_VEC3);
    if (environmentHalfExtentsLoc >= 0) SetShaderValue(shader, environmentHalfExtentsLoc, &environmentSelection.halfExtents, SHADER_UNIFORM_VEC3);
    if (environmentYawLoc >= 0) SetShaderValue(shader, environmentYawLoc, &environmentSelection.yawRadians, SHADER_UNIFORM_FLOAT);
    if (environmentMaxLodLoc >= 0) SetShaderValue(shader, environmentMaxLodLoc, &environmentSelection.maxLod, SHADER_UNIFORM_FLOAT);
    if (environmentIntensityLoc >= 0) SetShaderValue(shader, environmentIntensityLoc, &environmentSelection.intensity, SHADER_UNIFORM_FLOAT);
    if (useObjectProbeLightingLoc >= 0) SetShaderValue(shader, useObjectProbeLightingLoc, &useObjectProbe, SHADER_UNIFORM_INT);
    if (useVerticalObjectProbeLightingLoc >= 0) SetShaderValue(shader, useVerticalObjectProbeLightingLoc, &useVerticalProbe, SHADER_UNIFORM_INT);
}

void SectorStaticModelRenderer::UploadPbrMaterialTransferState(
        const engine::ModelMaterialAsset& material)
{
    const int baseHardwareSrgb = material.textureInfo[static_cast<size_t>(
            engine::ModelMaterialTextureRole::BaseColor)].hardwareSrgbDecode ? 1 : 0;
    const int emissiveHardwareSrgb = material.textureInfo[static_cast<size_t>(
            engine::ModelMaterialTextureRole::Emissive)].hardwareSrgbDecode ? 1 : 0;
    if (baseColorHardwareSrgbLoc >= 0) SetShaderValue(shader, baseColorHardwareSrgbLoc, &baseHardwareSrgb, SHADER_UNIFORM_INT);
    if (emissiveHardwareSrgbLoc >= 0) SetShaderValue(shader, emissiveHardwareSrgbLoc, &emissiveHardwareSrgb, SHADER_UNIFORM_INT);
}

void SectorStaticModelRenderer::RecordPbrDiagnostics(
        SectorPbrDrawDiagnostics& diagnostics,
        int placedObjectId,
        engine::ModelHandle model,
        int materialIndex,
        const SectorPbrDrawState& state,
        const engine::ModelMaterialAsset& material,
        const SectorStaticSpecularLightContext& staticSpecularLights)
{
    if (contributionSettings.reflectionCapture) return;
    diagnostics.valid = true;
    diagnostics.placedObjectId = placedObjectId;
    diagnostics.model = model;
    diagnostics.materialIndex = materialIndex;
    diagnostics.state = state;
    diagnostics.material = material;
    diagnostics.staticSpecularLights = staticSpecularLights;
    diagnostics.reflectionProbeIds={environmentBlend.first.probeId,environmentBlend.second.probeId};
    diagnostics.reflectionSecondWeight=SectorReflectionBlendWeight(environmentBlend,reflectionReceiverPosition);
    diagnostics.reflectionTransition={environmentBlend.first.transition,environmentBlend.second.transition};
    diagnostics.state.environmentActive=!engine::IsNull(environmentBlend.first.cubemap);
    if (environmentBlend.first.localProbe) diagnostics.state.environmentExposure=environmentBlend.first.intensity;
}

void SectorStaticModelRenderer::SetLightmapData(
        SectorStaticModelLightmapData data)
{
    ClearCachedModels();
    lightmapData = std::move(data);
    std::sort(
            lightmapData.objects.begin(),
            lightmapData.objects.end(),
            [](const auto& left, const auto& right) {
                return left.objectId < right.objectId;
            });
    cachedModels.reserve(lightmapData.models.size());
}

void SectorStaticModelRenderer::ClearCachedModels()
{
    for (CachedModel& cached : cachedModels) {
        for (Mesh& mesh : cached.meshes) {
            if (mesh.vaoId != 0
                    || mesh.vertices != nullptr
                    || mesh.vboId != nullptr) {
                UnloadMesh(mesh);
            }
            mesh = {};
        }
    }
    cachedModels.clear();
}

const SectorStaticModelRenderer::CachedModel*
SectorStaticModelRenderer::FindCachedModel(
        engine::ModelHandle handle,
        int lightmapModelIndex) const
{
    const auto found = std::find_if(
            cachedModels.begin(),
            cachedModels.end(),
            [handle, lightmapModelIndex](const CachedModel& cached) {
                return cached.handle == handle
                        && cached.lightmapModelIndex == lightmapModelIndex;
            });
    return found == cachedModels.end() ? nullptr : &*found;
}

void SectorStaticModelRenderer::FinalizeResources(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld)
{
    engine::PrepareAnimatedModelInstancesSystem(runtimeObjectWorld, assets);
    if (lightmapData.objects.empty()) {
        return;
    }
    runtimeObjectWorld.ForEach<
            SectorObject,
            SectorStaticModel>(
            [this, &assets](
                    engine::Entity,
                    SectorObject&,
                    SectorStaticModel& staticModel) {
                const SectorStaticModelLightmapObject* object =
                        FindLightmapObject(
                                lightmapData,
                                staticModel.placedObjectId);
                if (object == nullptr
                        || object->modelIndex < 0
                        || object->modelIndex
                                >= static_cast<int>(lightmapData.models.size())
                        || FindCachedModel(
                                   staticModel.model,
                                   object->modelIndex)
                                != nullptr) {
                    return;
                }
                const Model* source = assets.GetModel(staticModel.model);
                if (source == nullptr) {
                    return;
                }
                const auto& lightmapModel =
                        lightmapData.models[
                                static_cast<size_t>(object->modelIndex)];
                CachedModel cached;
                cached.handle = staticModel.model;
                cached.lightmapModelIndex = object->modelIndex;
                cached.meshes.resize(lightmapModel.meshes.size());
                bool valid = source->meshCount
                        == static_cast<int>(lightmapModel.meshes.size());
                for (size_t meshIndex = 0;
                        valid && meshIndex < lightmapModel.meshes.size();
                        ++meshIndex) {
                    valid = BuildRemappedMesh(
                            source->meshes[meshIndex],
                            lightmapModel.meshes[meshIndex],
                            cached.meshes[meshIndex]);
                }
                if (!valid) {
                    for (Mesh& mesh : cached.meshes) {
                        if (mesh.vaoId != 0
                                || mesh.vertices != nullptr
                                || mesh.vboId != nullptr) {
                            UnloadMesh(mesh);
                        }
                        mesh = {};
                    }
                    cached.meshes.clear();
                    if (!warningPrinted) {
                        std::fprintf(
                                stderr,
                                "[SectorMeshRenderer WARNING] Static prop lightmap mesh remap could not be finalized; using sector ambient fallback\n");
                        warningPrinted = true;
                    }
                }
                cachedModels.push_back(std::move(cached));
            });
}

void SectorStaticModelRenderer::ReserveShadowCasterCapacity(size_t capacity)
{
    staticDraws.reserve(capacity);
    modelDoorDraws.reserve(capacity);
    drawCapacityWarned = false;
    ReserveSectorStaticModelShadowCasters(
            shadowCasterCollection,
            capacity);
}

void SectorStaticModelRenderer::PrepareVisibleDraws(engine::AssetManager &assets,
                                                    engine::World &world, const Camera3D &camera,
                                                    float aspect,
                                                    const RuntimePortalVisibilityResult &visibility)
{
    staticDraws.clear();
    modelDoorDraws.clear();
    culledOpaqueObjects = submittedMeshes = submittedTriangles = 0;
    culledMeshes = culledTriangles = 0;
    const auto countCulled = [&](const engine::ModelAsset *asset) {
        if (!asset)
            return;
        culledMeshes += asset->model.meshCount;
        for (int i = 0; i < asset->model.meshCount; ++i)
            culledTriangles += asset->model.meshes[i].triangleCount;
    };
    world.ForEach<SectorObjectTransform, SectorObject, SectorStaticModel>(
        [&](engine::Entity entity, SectorObjectTransform &transform, SectorObject &object,
            SectorStaticModel &prop) {
            const auto *asset = assets.GetModelAsset(prop.model);
            if (!asset) {
                ++culledOpaqueObjects;
                return;
            }
            const Matrix authored = BuildSectorStaticModelAuthoredTransform(
                transform.position, transform.rotationXRadians, transform.yawRadians,
                transform.rotationZRadians, prop.scale);
            // ModelAsset bounds already include model.transform.
            const BoundingBox bounds =
                asset->hasLocalBounds ? TransformSectorDoorModelBounds(asset->localBounds, authored)
                                      : BoundingBox{transform.position, transform.position};
            if (!SectorOpaqueModelVisible(object.visible, object.currentSectorId, visibility,
                                          camera, aspect, rlGetCullDistanceNear(),
                                          rlGetCullDistanceFar(), bounds, asset->hasLocalBounds)) {
                ++culledOpaqueObjects;
                countCulled(asset);
                return;
            }
            AppendSectorOpaqueDraw(staticDraws,
                                   {entity, prop.placedObjectId, 0,
                                    MatrixMultiply(asset->model.transform, authored), bounds,
                                    SectorNearestViewDepth(camera, bounds)},
                                   drawCapacityWarned);
        });
    world.ForEach<SectorObject, SectorObjectLighting, SectorDoor, SectorDoorResolvedAnchor,
                  SectorDoorRender, SectorDoorModelRender>(
        [&](engine::Entity entity, SectorObject &object, SectorObjectLighting &, SectorDoor &door,
            SectorDoorResolvedAnchor &anchor, SectorDoorRender &render,
            SectorDoorModelRender &model) {
            if (!model.modelVisualRequested || !object.visible || !door.enabled || !render.visible)
                return;
            const bool inView = SectorBoundsInView(camera, aspect, rlGetCullDistanceNear(),
                                                   rlGetCullDistanceFar(), model.receiverBounds);
            if (!inView || !ShouldDrawSectorDoorForVisibility(anchor, visibility, inView)) {
                ++culledOpaqueObjects;
                countCulled(assets.GetModelAsset(model.leafModel));
                countCulled(assets.GetModelAsset(model.frameModel));
                return;
            }
            const auto policy = ResolveSectorDoorModelDrawPolicy(
                model, assets.GetModelAsset(model.leafModel) != nullptr,
                assets.GetModelAsset(model.frameModel) != nullptr);
            if (!policy.drawLeaf && !policy.drawFrame)
                return;
            AppendSectorOpaqueDraw(modelDoorDraws,
                                   {entity, door.placedObjectId, 0, MatrixIdentity(),
                                    model.receiverBounds,
                                    SectorNearestViewDepth(camera, model.receiverBounds)},
                                   drawCapacityWarned);
        });
    std::sort(staticDraws.begin(), staticDraws.end(), SectorOpaqueDrawLess);
    std::sort(modelDoorDraws.begin(), modelDoorDraws.end(), SectorOpaqueDrawLess);
}

void SectorStaticModelRenderer::DrawPreparedDepth(
    engine::AssetManager &assets, engine::World &world, Material depthMaterial,
    const std::vector<engine::TextureHandle> &lightmapTextures)
{
    if (!shaderLoaded)
        return;
    const auto drawModel = [&](const engine::ModelAsset &asset, engine::ModelHandle handle,
                               Matrix transform, const SectorStaticModelLightmapObject *object) {
        const Model &model = asset.model;
        const CachedModel *cached = object ? FindCachedModel(handle, object->modelIndex) : nullptr;
        for (int i = 0; i < model.meshCount; ++i) {
            if (!model.meshMaterial)
                continue;
            const int materialIndex = model.meshMaterial[i];
            if (materialIndex < 0 || materialIndex >= model.materialCount ||
                !model.materials[materialIndex].maps ||
                materialIndex >= static_cast<int>(asset.materials.size()))
                continue;
            const auto &material = asset.materials[materialIndex];
            if (!SectorMaterialDepthEligible(material))
                continue;
            const Mesh *mesh = &model.meshes[i];
            if (cached && object && cached->meshes.size() == static_cast<size_t>(model.meshCount) &&
                object->meshPlacements.size() == static_cast<size_t>(model.meshCount)) {
                const int atlas = object->meshPlacements[i].atlasIndex;
                const Texture2D *lightmap =
                    atlas >= 0 && atlas < static_cast<int>(lightmapTextures.size())
                        ? assets.GetTexture(lightmapTextures[atlas])
                        : nullptr;
                if (lightmap && lightmap->id)
                    mesh = &cached->meshes[i];
            }
            ApplySectorMaterialCulling(material, transform);
            DrawMesh(*mesh, depthMaterial, transform);
        }
    };
    for (const auto &item : staticDraws) {
        if (!world.IsAlive(item.entity) || !world.Has<SectorStaticModel>(item.entity))
            continue;
        const auto &prop = world.Get<SectorStaticModel>(item.entity);
        const auto *asset = assets.GetModelAsset(prop.model);
        if (asset)
            drawModel(*asset, prop.model, item.transform,
                      FindLightmapObject(lightmapData, prop.placedObjectId));
    }
    for (const auto &item : modelDoorDraws) {
        if (!world.IsAlive(item.entity) || !world.Has<SectorDoorModelRender>(item.entity))
            continue;
        const auto &door = world.Get<SectorDoorModelRender>(item.entity);
        const auto *leaf = assets.GetModelAsset(door.leafModel);
        const auto *frame = assets.GetModelAsset(door.frameModel);
        const auto policy =
            ResolveSectorDoorModelDrawPolicy(door, leaf != nullptr, frame != nullptr);
        if (policy.drawLeaf)
            drawModel(*leaf, door.leafModel, MatrixMultiply(leaf->model.transform, door.leafMatrix),
                      nullptr);
        if (policy.drawFrame)
            drawModel(*frame, door.frameModel,
                      MatrixMultiply(frame->model.transform, door.frameMatrix), nullptr);
    }
    RestoreSectorMaterialCulling();
}

void SectorStaticModelRenderer::PrepareShadowRenderContext(
        SectorDynamicSpotLightShadowRenderContext& context,
        engine::World* runtimeObjectWorld)
{
    UpdateSectorStaticModelShadowCasters(
            shadowCasterCollection,
            runtimeObjectWorld);
    context.staticModelShadowCasters = &shadowCasterCollection.casters;
    context.staticModelShadowCasterRevision =
            shadowCasterCollection.revision;
}

void SectorStaticModelRenderer::ClearPreparedShadowCasters()
{
    ClearSectorStaticModelShadowCasters(shadowCasterCollection);
}

bool SectorStaticModelRenderer::DrawWorldDynamicModel(
        const engine::ModelAsset& modelAsset,
        const Model& model,
        engine::ModelHandle modelHandle,
        Matrix modelTransform,
        int placedObjectId,
        int receiverSectorId,
        const SectorReceiverBounds& receiverBounds,
        Vector3 containingSectorAmbient,
        float environmentExposure,
        const BakedObjectLightingVerticalSample& lighting,
        const SectorBillboardDynamicLightContext& dynamicLightContext,
        const SectorStaticSpecularLightState& staticSpecularLights,
        const RuntimePortalVisibilityResult& visibility,
        bool objectProbeBakeCurrent,
        const TextureCubemap* environment,
        bool allowSkinning,
        const engine::AnimatedModelInstance* animatedInstance,
        const std::vector<Matrix>* meshNodeMatrices,
        float emissiveScale,
        float opacity,
        float interactionHighlightStrength)
{
    if (captureCulling && !AcceptSectorReflectionObject(captureCulling,
            TransformSectorDoorModelBounds(modelAsset.localBounds, modelTransform),
            modelAsset.hasLocalBounds)) return false;
    PrepareReceiverEnvironment(Vector3Scale(Vector3Add(receiverBounds.min,receiverBounds.max),0.5f),receiverSectorId,&receiverBounds);
    const int noStaticLightmap = 0;
    const int noBakedAo = 0;
    opacity = std::isfinite(opacity) ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
    if (modelOpacityLoc >= 0) {
        SetShaderValue(shader, modelOpacityLoc, &opacity, SHADER_UNIFORM_FLOAT);
    }
    UploadInteractionHighlightStrength(interactionHighlightStrength);
    if (hasStaticLightmapLoc >= 0) SetShaderValue(shader, hasStaticLightmapLoc, &noStaticLightmap, SHADER_UNIFORM_INT);
    if (useBakedAmbientOcclusionLoc >= 0) SetShaderValue(shader, useBakedAmbientOcclusionLoc, &noBakedAo, SHADER_UNIFORM_INT);
    if (containingSectorAmbientLoc >= 0) {
        const Vector3 ambient = SanitizeSectorPbrNonnegative(containingSectorAmbient);
        SetShaderValue(shader, containingSectorAmbientLoc, &ambient, SHADER_UNIFORM_VEC3);
    }
    for (size_t face = 0; face < objectAmbientCubeLocs.size(); ++face) {
        if (objectAmbientCubeLocs[face] >= 0) {
            const Vector3 lowerAmbient = SanitizeSectorPbrNonnegative(
                    lighting.lower.ambientCube[face]);
            SetShaderValue(shader, objectAmbientCubeLocs[face], &lowerAmbient, SHADER_UNIFORM_VEC3);
        }
        if (objectAmbientCubeUpperLocs[face] >= 0) {
            const Vector3 upperAmbient = SanitizeSectorPbrNonnegative(
                    lighting.upper.ambientCube[face]);
            SetShaderValue(shader, objectAmbientCubeUpperLocs[face], &upperAmbient, SHADER_UNIFORM_VEC3);
        }
    }
    const float lowerProbeHeight = std::isfinite(lighting.lowerHeightWorld)
            ? lighting.lowerHeightWorld : 0.0f;
    const float upperProbeHeight = std::isfinite(lighting.upperHeightWorld)
            ? lighting.upperHeightWorld : lowerProbeHeight;
    if (objectAmbientCubeLowerHeightLoc >= 0) SetShaderValue(shader, objectAmbientCubeLowerHeightLoc, &lowerProbeHeight, SHADER_UNIFORM_FLOAT);
    if (objectAmbientCubeUpperHeightLoc >= 0) SetShaderValue(shader, objectAmbientCubeUpperHeightLoc, &upperProbeHeight, SHADER_UNIFORM_FLOAT);
    const bool validProbe = lighting.lower.valid || lighting.upper.valid;
    const SectorStaticSpecularLightContext staticSpecularContext =
            SelectSectorStaticSpecularLights(
                    staticSpecularLights,
                    receiverBounds,
                    receiverSectorId,
                    visibility,
                    objectProbeBakeCurrent && validProbe);
    UploadSectorStaticSpecularLights(
            shader, staticSpecularLocations, staticSpecularContext);

    const bool environmentActive = environment != nullptr && environment->id != 0;
    bool drewMesh = false;
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (model.meshMaterial == nullptr) continue;
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) continue;
        const Material& source = model.materials[materialIndex];
        if (source.maps == nullptr) continue;

        int meshBoneCount = 0;
        const Matrix* meshBoneMatrices = nullptr;
        if (allowSkinning && animatedInstance != nullptr) {
            meshBoneMatrices = engine::AnimatedModelMeshBoneMatrices(
                    modelAsset,
                    *animatedInstance,
                    meshIndex,
                    meshBoneCount);
        } else if (allowSkinning
                && model.skeleton.boneCount > 0
                && model.skeleton.boneCount
                        <= engine::MaxAnimatedModelBones
                && model.boneMatrices != nullptr) {
            meshBoneMatrices = model.boneMatrices;
            meshBoneCount = model.skeleton.boneCount;
        }
        const bool canSkin = meshBoneMatrices != nullptr
                && meshBoneCount > 0
                && shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] >= 0;
        const int useSkinning = canSkin ? 1 : 0;
        if (useSkinningLoc >= 0) {
            SetShaderValue(
                    shader,
                    useSkinningLoc,
                    &useSkinning,
                    SHADER_UNIFORM_INT);
        }
        if (canSkin) {
            rlEnableShader(shader.id);
            rlSetUniformMatrices(
                    shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
                    meshBoneMatrices,
                    meshBoneCount);
        }

        std::array<MaterialMap, SectorStaticModelMaterialMapCount> maps{};
        std::copy_n(source.maps, SectorStaticModelMaterialMapCount, maps.begin());
        ConfigureSectorStaticModelAuxiliaryMaterialMaps(
                maps,
                nullptr,
                false,
                environment,
                dynamicLightContext.shadowMaps.shadowMap0,
                dynamicLightContext.shadowMaps.shadowMap1);
        Material material = source;
        material.shader = shader;
        material.maps = maps.data();
        engine::ModelMaterialAsset pbrMaterial;
        if (materialIndex < static_cast<int>(modelAsset.materials.size())
                && modelAsset.materials[static_cast<size_t>(materialIndex)]
                           .pbrMetallicRoughness) {
            pbrMaterial = modelAsset.materials[static_cast<size_t>(materialIndex)];
        } else {
            pbrMaterial.baseColorFactor = ColorToNormalizedVector4(
                    maps[MATERIAL_MAP_DIFFUSE].color);
            pbrMaterial.roughnessFactor = 1.0f;
            pbrMaterial.hasBaseColorTexture =
                    maps[MATERIAL_MAP_DIFFUSE].texture.id != 0;
        }
        pbrMaterial = NormalizeSectorPbrMaterial(pbrMaterial);
        pbrMaterial.emissiveStrength = ScaleSectorPbrEmissiveStrength(
                pbrMaterial.emissiveStrength,
                emissiveScale);
        if (baseColorFactorLoc >= 0) SetShaderValue(shader, baseColorFactorLoc, &pbrMaterial.baseColorFactor, SHADER_UNIFORM_VEC4);
        if (emissiveFactorLoc >= 0) SetShaderValue(shader, emissiveFactorLoc, &pbrMaterial.emissiveFactor, SHADER_UNIFORM_VEC3);
        if (emissiveStrengthLoc >= 0) SetShaderValue(shader, emissiveStrengthLoc, &pbrMaterial.emissiveStrength, SHADER_UNIFORM_FLOAT);
        if (metallicFactorLoc >= 0) SetShaderValue(shader, metallicFactorLoc, &pbrMaterial.metallicFactor, SHADER_UNIFORM_FLOAT);
        if (roughnessFactorLoc >= 0) SetShaderValue(shader, roughnessFactorLoc, &pbrMaterial.roughnessFactor, SHADER_UNIFORM_FLOAT);
        if (normalScaleLoc >= 0) SetShaderValue(shader, normalScaleLoc, &pbrMaterial.normalScale, SHADER_UNIFORM_FLOAT);
        if (occlusionStrengthLoc >= 0) SetShaderValue(shader, occlusionStrengthLoc, &pbrMaterial.occlusionStrength, SHADER_UNIFORM_FLOAT);
        const int hasBase = pbrMaterial.hasBaseColorTexture ? 1 : 0;
        const int hasMetal = pbrMaterial.hasMetallicTexture ? 1 : 0;
        const int hasNormal = pbrMaterial.hasNormalTexture ? 1 : 0;
        const int hasRoughness = pbrMaterial.hasRoughnessTexture ? 1 : 0;
        const int hasOcclusion = pbrMaterial.hasOcclusionTexture ? 1 : 0;
        const int hasEmissive = pbrMaterial.hasEmissiveTexture ? 1 : 0;
        if (hasBaseColorTextureLoc >= 0) SetShaderValue(shader, hasBaseColorTextureLoc, &hasBase, SHADER_UNIFORM_INT);
        if (hasMetallicTextureLoc >= 0) SetShaderValue(shader, hasMetallicTextureLoc, &hasMetal, SHADER_UNIFORM_INT);
        if (hasNormalTextureLoc >= 0) SetShaderValue(shader, hasNormalTextureLoc, &hasNormal, SHADER_UNIFORM_INT);
        if (hasRoughnessTextureLoc >= 0) SetShaderValue(shader, hasRoughnessTextureLoc, &hasRoughness, SHADER_UNIFORM_INT);
        if (hasOcclusionTextureLoc >= 0) SetShaderValue(shader, hasOcclusionTextureLoc, &hasOcclusion, SHADER_UNIFORM_INT);
        if (hasEmissiveTextureLoc >= 0) SetShaderValue(shader, hasEmissiveTextureLoc, &hasEmissive, SHADER_UNIFORM_INT);
        const SectorPbrDrawState drawState = BuildSectorPbrDrawState(
                SectorPbrLightingPath::WorldDynamic,
                validProbe,
                false,
                objectProbeBakeCurrent,
                environmentActive,
                environmentExposure,
                1.0f,
                false,
                contributionSettings);
        UploadPbrDrawState(drawState);
        UploadPbrMaterialTransferState(pbrMaterial);
        if (!worldDiagnostics.valid
                && (placedObjectId == diagnosticSelectedObjectId
                        || diagnosticSelectedObjectId < 0)) {
            RecordPbrDiagnostics(
                    worldDiagnostics,
                    placedObjectId,
                    modelHandle,
                    materialIndex,
                    drawState,
                    pbrMaterial,
                    staticSpecularContext);
        }
        const Matrix meshTransform = meshNodeMatrices != nullptr
                        && static_cast<size_t>(meshIndex)
                                < meshNodeMatrices->size()
                ? MatrixMultiply(
                        (*meshNodeMatrices)[
                                static_cast<size_t>(meshIndex)],
                        modelTransform)
                : modelTransform;
        if (drawingModelDoor) ApplySectorMaterialCulling(
                materialIndex < static_cast<int>(modelAsset.materials.size())
                        ? modelAsset.materials[materialIndex] : engine::ModelMaterialAsset{}, meshTransform);
        DrawMesh(model.meshes[meshIndex], material, meshTransform);
        if (drawingModelDoor) {
            RestoreSectorMaterialCulling();
            rlDisableBackfaceCulling();
            ++submittedMeshes;
            submittedTriangles += model.meshes[meshIndex].triangleCount;
        }
        drewMesh = true;
    }
    return drewMesh;
}

void SectorStaticModelRenderer::UploadInteractionHighlightStrength(
        float strength)
{
    strength = std::isfinite(strength)
            ? std::clamp(strength, 0.0f, 0.14f)
            : 0.0f;
    if (interactionHighlightStrengthLoc >= 0) {
        SetShaderValue(
                shader,
                interactionHighlightStrengthLoc,
                &strength,
                SHADER_UNIFORM_FLOAT);
    }
}

void SectorStaticModelRenderer::Draw(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLightContext,
        const SectorStaticSpecularLightState& staticSpecularLights,
        bool surfaceLightmapBakeCurrent,
        bool objectProbeBakeCurrent,
        const SectorFogRenderContext& fogContext,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<engine::TextureHandle>& lightmapTextures,
        const TextureCubemap* environment,
        bool useBakedAmbientOcclusion,
        std::string& renderDebugText,
        bool staticCaptureOnly,
        SectorUseHighlight useHighlight,
        SectorReflectionCaptureCulling* captureCulling)
{
    this->captureCulling = captureCulling;
    drawAssets=&assets;
    if (!shaderLoaded || shader.id == 0) {
        AppendStaticModelDebugText(renderDebugText, 0, 0, 0, 0);
        return;
    }
    const float opaque = 1.0f;
    if (modelOpacityLoc >= 0) {
        SetShaderValue(shader, modelOpacityLoc, &opaque, SHADER_UNIFORM_FLOAT);
    }
    UploadInteractionHighlightStrength(0.0f);

    SectorDynamicLightShaderLocations dynamicLocations;
    dynamicLocations.dynamicLightCount = dynamicLightCountLoc;
    dynamicLocations.dynamicLightPositions = dynamicLightPositionsLoc;
    dynamicLocations.dynamicLightColors = dynamicLightColorsLoc;
    dynamicLocations.dynamicLightRadii = dynamicLightRadiiLoc;
    dynamicLocations.dynamicLightIntensities = dynamicLightIntensitiesLoc;
    dynamicLocations.dynamicLightTypes = dynamicLightTypesLoc;
    dynamicLocations.dynamicLightDirections = dynamicLightDirectionsLoc;
    dynamicLocations.dynamicLightInnerConeCos = dynamicLightInnerConeCosLoc;
    dynamicLocations.dynamicLightOuterConeCos = dynamicLightOuterConeCosLoc;
    dynamicLocations.dynamicLightSpotShadowRight = dynamicLightSpotShadowRightLoc;
    dynamicLocations.dynamicLightSpotShadowProjection =
            dynamicLightSpotShadowProjectionLoc;
    dynamicLocations.dynamicLightProfiles = dynamicLightProfilesLoc;
    dynamicLocations.dynamicLightProfileParameters =
            dynamicLightProfileParametersLoc;
    dynamicLocations.flashlightCookie = flashlightCookieLoc;
    dynamicLocations.hasPointShadows = hasPointShadowsLoc;
    UploadSectorRendererDynamicPointLights(
            shader,
            dynamicLocations,
            dynamicLightContext);

    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    shadowLocations.shadowAtlasTilesPerRow = shadowAtlasTilesPerRowLoc;
    UploadSectorRendererDynamicSpotLightShadowUniforms(
            shader,
            shadowLocations,
            dynamicLightContext.shadowUniforms);
    UploadSectorFogShaderValues(shader, fogShaderLocations, fogContext);
    if (cameraPositionLoc >= 0) {
        SetShaderValue(
                shader,
                cameraPositionLoc,
                &camera.position,
                SHADER_UNIFORM_VEC3);
    }
    const bool environmentActive = environment != nullptr && environment->id != 0;
    const int viewWidth = GetScreenWidth();
    const int viewHeight = GetScreenHeight();
    const float viewAspect = viewWidth > 0 && viewHeight > 0
            ? static_cast<float>(viewWidth) / static_cast<float>(viewHeight)
            : 0.0f;
    const float viewNearPlane = static_cast<float>(rlGetCullDistanceNear());
    if (!staticCaptureOnly) worldDiagnostics = {};

    size_t considered = 0;
    size_t drawn = 0;
    size_t portalCulled = 0;
    size_t skipped = 0;
    rlDisableColorBlend();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorObjectLighting,
            SectorItem>(
            [this,
             &assets,
             &dynamicLightContext,
             &staticSpecularLights,
             &visibility,
             environment,
             objectProbeBakeCurrent,
             &considered,
             &drawn,
             &portalCulled,
             &skipped,
             &runtimeObjectWorld,
             staticCaptureOnly,
             useHighlight](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorObjectLighting& lighting,
                    SectorItem& item) {
                if (staticCaptureOnly) return;
                ++considered;
                if (!ShouldDrawRuntimeSectorForVisibility(
                            object.currentSectorId, visibility)) {
                    ++portalCulled;
                    return;
                }
                if (!object.visible) {
                    ++skipped;
                    return;
                }
                const engine::ModelAsset* asset = assets.GetModelAsset(item.model);
                if (asset == nullptr) {
                    ++skipped;
                    return;
                }
                Vector3 renderPosition = transform.position;
                if (runtimeObjectWorld.Has<SectorObjectVisualOffset>(entity)) {
                    renderPosition = Vector3Add(
                            renderPosition,
                            runtimeObjectWorld
                                    .Get<SectorObjectVisualOffset>(entity)
                                    .position);
                }
                const float renderScale = item.scale
                        * item.presentation.scaleMultiplier;
                const Matrix authoredTransform =
                        BuildSectorStaticModelAuthoredTransform(
                                renderPosition,
                                transform.rotationXRadians,
                                transform.yawRadians,
                                transform.rotationZRadians,
                                renderScale);
                const Matrix modelTransform = MatrixMultiply(
                        asset->model.transform, authoredTransform);
                const SectorReceiverBounds receiverBounds = asset->hasLocalBounds
                        ? TransformSectorStaticSpecularReceiverBounds(
                                asset->localBounds,
                                authoredTransform,
                                object.currentSectorId,
                                renderPosition)
                        : SectorReceiverBounds{
                                object.currentSectorId,
                                renderPosition,
                                renderPosition};
                const bool drewMesh = DrawWorldDynamicModel(
                        *asset,
                        asset->model,
                        item.model,
                        modelTransform,
                        item.placedObjectId,
                        object.currentSectorId,
                        receiverBounds,
                        item.containingSectorAmbient,
                        item.environmentExposure,
                        lighting.vertical,
                        dynamicLightContext,
                        staticSpecularLights,
                        visibility,
                        objectProbeBakeCurrent,
                        environment,
                        false,
                        nullptr,
                        nullptr,
                        1.0f,
                        1.0f,
                        entity == useHighlight.entity
                                ? useHighlight.strength : 0.0f);
                if (drewMesh) ++drawn;
                else ++skipped;
            });

    // The item pass sets a shared shader uniform per entity. Static props set
    // it explicitly below, so clear it before entering that pass.
    UploadInteractionHighlightStrength(0.0f);

    const auto drawStatic =
            [this,
             &assets,
             &dynamicLightContext,
             &staticSpecularLights,
             &visibility,
             &lightmapTextures,
             environment,
             environmentActive,
             surfaceLightmapBakeCurrent,
             useBakedAmbientOcclusion,
             staticCaptureOnly,
             useHighlight,
             &considered,
             &drawn,
             &portalCulled,
             &skipped](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorStaticModel& staticModel) {
                UploadInteractionHighlightStrength(
                        !staticCaptureOnly && entity == useHighlight.entity
                                ? useHighlight.strength : 0.0f);
                ++considered;
                if (!ShouldDrawRuntimeSectorForVisibility(
                            object.currentSectorId,
                            visibility)) {
                    ++portalCulled;
                    return;
                }
                if (!object.visible) {
                    ++skipped;
                    return;
                }
                const engine::ModelAsset* modelAsset =
                        assets.GetModelAsset(staticModel.model);
                if (modelAsset == nullptr) {
                    ++skipped;
                    if (!warningPrinted
                            && !engine::IsNull(staticModel.model)
                            && assets.HasFailed(staticModel.model)) {
                        std::fprintf(
                                stderr,
                                "[SectorMeshRenderer WARNING] Skipping static prop with failed model asset\n");
                        warningPrinted = true;
                    }
                    return;
                }
                const Model* model = &modelAsset->model;
                const Matrix authoredTransform = BuildSectorStaticModelAuthoredTransform(
                        transform.position, transform.rotationXRadians, transform.yawRadians,
                        transform.rotationZRadians, staticModel.scale);
                const Matrix modelTransform = MatrixMultiply(model->transform, authoredTransform);
                if (this->captureCulling && !AcceptSectorReflectionObject(this->captureCulling,
                        TransformSectorDoorModelBounds(modelAsset->localBounds, authoredTransform),
                        modelAsset->hasLocalBounds)) return;

                const int useSkinning = 0;
                if (useSkinningLoc >= 0) {
                    SetShaderValue(shader, useSkinningLoc, &useSkinning, SHADER_UNIFORM_INT);
                }

                if (containingSectorAmbientLoc >= 0) {
                    const Vector3 ambient = SanitizeSectorPbrNonnegative(
                            staticModel.containingSectorAmbient);
                    SetShaderValue(
                            shader,
                            containingSectorAmbientLoc,
                            &ambient,
                            SHADER_UNIFORM_VEC3);
                }
                const int useAo = useBakedAmbientOcclusion ? 1 : 0;
                if (useBakedAmbientOcclusionLoc >= 0) {
                    SetShaderValue(
                            shader,
                            useBakedAmbientOcclusionLoc,
                            &useAo,
                            SHADER_UNIFORM_INT);
                }
                const SectorStaticModelLightmapObject* lightmapObject =
                        FindLightmapObject(
                                lightmapData,
                                staticModel.placedObjectId);
                const CachedModel* cached =
                        lightmapObject == nullptr
                        ? nullptr
                        : FindCachedModel(
                                staticModel.model,
                                lightmapObject->modelIndex);
                const bool hasRemapData = lightmapObject != nullptr
                        && cached != nullptr
                        && cached->meshes.size()
                                == static_cast<size_t>(model->meshCount)
                        && lightmapObject->meshPlacements.size()
                                == static_cast<size_t>(model->meshCount);
                const SectorReceiverBounds receiverBounds =
                        modelAsset->hasLocalBounds
                        ? TransformSectorStaticSpecularReceiverBounds(
                                modelAsset->localBounds,
                                authoredTransform,
                                object.currentSectorId,
                                transform.position)
                        : SectorReceiverBounds{
                                object.currentSectorId,
                                transform.position,
                                transform.position};
                const SectorStaticSpecularLightContext staticSpecularContext =
                        SelectSectorStaticSpecularLights(
                                staticSpecularLights,
                                receiverBounds,
                                object.currentSectorId,
                                visibility,
                                surfaceLightmapBakeCurrent && hasRemapData);
                PrepareReceiverEnvironment(Vector3Scale(Vector3Add(receiverBounds.min,receiverBounds.max),0.5f),
                        object.currentSectorId,&receiverBounds);
                UploadSectorStaticSpecularLights(
                        shader,
                        staticSpecularLocations,
                        staticSpecularContext);

                bool drewMesh = false;
                for (int meshIndex = 0; meshIndex < model->meshCount; ++meshIndex) {
                    if (model->meshMaterial == nullptr) {
                        continue;
                    }
                    const int materialIndex = model->meshMaterial[meshIndex];
                    if (materialIndex < 0 || materialIndex >= model->materialCount) {
                        continue;
                    }
                    const Material& source = model->materials[materialIndex];
                    if (source.maps == nullptr) {
                        continue;
                    }

                    const SectorStaticModelLightmapMeshPlacement* placement =
                            hasRemapData
                            ? &lightmapObject->meshPlacements[
                                    static_cast<size_t>(meshIndex)]
                            : nullptr;
                    const Texture2D* lightmap = placement != nullptr
                            && placement->atlasIndex >= 0
                            && placement->atlasIndex
                                    < static_cast<int>(lightmapTextures.size())
                            ? assets.GetTexture(lightmapTextures[
                                    static_cast<size_t>(placement->atlasIndex)])
                            : nullptr;
                    const bool hasRemappedMesh = placement != nullptr
                            && lightmap != nullptr
                            && lightmap->id != 0;

                    std::array<
                            MaterialMap,
                            SectorStaticModelMaterialMapCount> maps{};
                    std::copy_n(
                            source.maps,
                            SectorStaticModelMaterialMapCount,
                            maps.begin());
                    ConfigureSectorStaticModelAuxiliaryMaterialMaps(
                            maps,
                            lightmap,
                            hasRemappedMesh,
                            environment,
                            dynamicLightContext.shadowMaps.shadowMap0,
                            dynamicLightContext.shadowMaps.shadowMap1);
                    Material material = source;
                    material.shader = shader;
                    material.maps = maps.data();
                    engine::ModelMaterialAsset pbrMaterial;
                    if (materialIndex >= 0
                            && materialIndex < static_cast<int>(modelAsset->materials.size())
                            && modelAsset->materials[
                                       static_cast<size_t>(materialIndex)]
                                       .pbrMetallicRoughness) {
                        pbrMaterial = modelAsset->materials[
                                static_cast<size_t>(materialIndex)];
                    } else {
                        pbrMaterial.baseColorFactor = ColorToNormalizedVector4(
                                maps[MATERIAL_MAP_DIFFUSE].color);
                        pbrMaterial.metallicFactor = 0.0f;
                        pbrMaterial.roughnessFactor = 1.0f;
                        pbrMaterial.hasBaseColorTexture =
                                maps[MATERIAL_MAP_DIFFUSE].texture.id != 0;
                    }
                    pbrMaterial = NormalizeSectorPbrMaterial(pbrMaterial);
                    pbrMaterial.emissiveStrength = ScaleSectorPbrEmissiveStrength(
                            pbrMaterial.emissiveStrength,
                            staticModel.emissiveScale);
                    if (baseColorFactorLoc >= 0) {
                        SetShaderValue(
                                shader,
                                baseColorFactorLoc,
                                &pbrMaterial.baseColorFactor,
                                SHADER_UNIFORM_VEC4);
                    }
                    if (emissiveFactorLoc >= 0) SetShaderValue(shader, emissiveFactorLoc, &pbrMaterial.emissiveFactor, SHADER_UNIFORM_VEC3);
                    if (emissiveStrengthLoc >= 0) SetShaderValue(shader, emissiveStrengthLoc, &pbrMaterial.emissiveStrength, SHADER_UNIFORM_FLOAT);
                    if (metallicFactorLoc >= 0) SetShaderValue(shader, metallicFactorLoc, &pbrMaterial.metallicFactor, SHADER_UNIFORM_FLOAT);
                    if (roughnessFactorLoc >= 0) SetShaderValue(shader, roughnessFactorLoc, &pbrMaterial.roughnessFactor, SHADER_UNIFORM_FLOAT);
                    if (normalScaleLoc >= 0) SetShaderValue(shader, normalScaleLoc, &pbrMaterial.normalScale, SHADER_UNIFORM_FLOAT);
                    if (occlusionStrengthLoc >= 0) SetShaderValue(shader, occlusionStrengthLoc, &pbrMaterial.occlusionStrength, SHADER_UNIFORM_FLOAT);
                    const int hasBase = pbrMaterial.hasBaseColorTexture ? 1 : 0;
                    const int hasMetal = pbrMaterial.hasMetallicTexture ? 1 : 0;
                    const int hasNormal = pbrMaterial.hasNormalTexture ? 1 : 0;
                    const int hasRoughness = pbrMaterial.hasRoughnessTexture ? 1 : 0;
                    const int hasOcclusion = pbrMaterial.hasOcclusionTexture ? 1 : 0;
                    const int hasEmissive = pbrMaterial.hasEmissiveTexture ? 1 : 0;
                    if (hasBaseColorTextureLoc >= 0) SetShaderValue(shader, hasBaseColorTextureLoc, &hasBase, SHADER_UNIFORM_INT);
                    if (hasMetallicTextureLoc >= 0) SetShaderValue(shader, hasMetallicTextureLoc, &hasMetal, SHADER_UNIFORM_INT);
                    if (hasNormalTextureLoc >= 0) SetShaderValue(shader, hasNormalTextureLoc, &hasNormal, SHADER_UNIFORM_INT);
                    if (hasRoughnessTextureLoc >= 0) SetShaderValue(shader, hasRoughnessTextureLoc, &hasRoughness, SHADER_UNIFORM_INT);
                    if (hasOcclusionTextureLoc >= 0) SetShaderValue(shader, hasOcclusionTextureLoc, &hasOcclusion, SHADER_UNIFORM_INT);
                    if (hasEmissiveTextureLoc >= 0) SetShaderValue(shader, hasEmissiveTextureLoc, &hasEmissive, SHADER_UNIFORM_INT);
                    const int hasStaticLightmap =
                            hasRemappedMesh ? 1 : 0;
                    if (hasStaticLightmapLoc >= 0) {
                        SetShaderValue(
                                shader,
                                hasStaticLightmapLoc,
                                &hasStaticLightmap,
                                SHADER_UNIFORM_INT);
                    }
                    Vector4 scaleBias{};
                    if (hasRemappedMesh) {
                        scaleBias = Vector4{
                                placement->atlasScale.x,
                                placement->atlasScale.y,
                                placement->atlasBias.x,
                                placement->atlasBias.y};
                    }
                    if (lightmapScaleBiasLoc >= 0) {
                        SetShaderValue(
                                shader,
                                lightmapScaleBiasLoc,
                                &scaleBias,
                                SHADER_UNIFORM_VEC4);
                    }
                    const SectorPbrDrawState drawState = BuildSectorPbrDrawState(
                            SectorPbrLightingPath::WorldStatic,
                            false,
                            hasRemappedMesh,
                            surfaceLightmapBakeCurrent,
                            environmentActive,
                            staticModel.environmentExposure,
                            1.0f,
                            false,
                            contributionSettings);
                    UploadPbrDrawState(drawState);
                    UploadPbrMaterialTransferState(pbrMaterial);
                    if (!worldDiagnostics.valid
                            && (staticModel.placedObjectId
                                            == diagnosticSelectedObjectId
                                    || diagnosticSelectedObjectId < 0)) {
                        RecordPbrDiagnostics(
                                worldDiagnostics,
                                staticModel.placedObjectId,
                                staticModel.model,
                                materialIndex,
                                drawState,
                                pbrMaterial,
                                staticSpecularContext);
                    }
                    const Mesh& mesh = hasRemappedMesh
                            ? cached->meshes[static_cast<size_t>(meshIndex)]
                            : model->meshes[meshIndex];
                    ApplySectorMaterialCulling(
                            materialIndex < static_cast<int>(modelAsset->materials.size())
                                    ? modelAsset->materials[materialIndex] : engine::ModelMaterialAsset{}, modelTransform);
                    DrawMesh(mesh, material, modelTransform);
                    RestoreSectorMaterialCulling();
                    rlDisableBackfaceCulling();
                    ++submittedMeshes;
                    submittedTriangles += mesh.triangleCount;
                    drewMesh = true;
                }
                if (drewMesh) {
                    ++drawn;
                } else {
                    ++skipped;
                }
            };
    for (const auto& item : staticDraws) {
        if (!runtimeObjectWorld.IsAlive(item.entity)) continue;
        drawStatic(item.entity, runtimeObjectWorld.Get<SectorObjectTransform>(item.entity),
                runtimeObjectWorld.Get<SectorObject>(item.entity),
                runtimeObjectWorld.Get<SectorStaticModel>(item.entity));
    }

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorObjectLighting,
            SectorDynamicModel,
            engine::AnimatedModelInstance>(
            [this,
             &assets,
             &dynamicLightContext,
             &staticSpecularLights,
             &visibility,
             environment,
             objectProbeBakeCurrent,
             &considered,
             &drawn,
             &portalCulled,
             &skipped,
             staticCaptureOnly,
             &runtimeObjectWorld,
             useHighlight](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorObjectLighting& lighting,
                    SectorDynamicModel& dynamicModel,
                    engine::AnimatedModelInstance& instance) {
                if (staticCaptureOnly) return;
                ++considered;
                if (!ShouldDrawRuntimeSectorForVisibility(object.currentSectorId, visibility)) {
                    ++portalCulled;
                    return;
                }
                if (!object.visible || !instance.poseReady || instance.poseFailed) {
                    ++skipped;
                    return;
                }
                const engine::ModelAsset* modelAsset = assets.GetModelAsset(instance.model);
                if (modelAsset == nullptr) {
                    ++skipped;
                    return;
                }
                Model model = engine::BuildAnimatedModelPoseView(*modelAsset, instance);
                Vector3 renderPosition = transform.position;
                if (runtimeObjectWorld.Has<SectorObjectVisualOffset>(entity)) {
                    renderPosition = Vector3Add(
                            renderPosition,
                            runtimeObjectWorld.Get<SectorObjectVisualOffset>(entity).position);
                }
                const Matrix authoredTransform = BuildSectorStaticModelAuthoredTransform(
                        renderPosition,
                        transform.rotationXRadians,
                        transform.yawRadians,
                        transform.rotationZRadians,
                        dynamicModel.scale);
                const Matrix modelTransform = MatrixMultiply(model.transform, authoredTransform);
                const BoundingBox* receiverLocalBounds =
                        modelAsset->hasAnimatedLocalBounds
                        ? &modelAsset->animatedLocalBounds
                        : (modelAsset->hasLocalBounds
                                ? &modelAsset->localBounds
                                : nullptr);
                const SectorReceiverBounds receiverBounds =
                        receiverLocalBounds != nullptr
                        ? TransformSectorStaticSpecularReceiverBounds(
                                *receiverLocalBounds,
                                authoredTransform,
                                object.currentSectorId,
                                renderPosition)
                        : SectorReceiverBounds{
                                object.currentSectorId,
                                renderPosition,
                                renderPosition};
                const bool fading = dynamicModel.opacity < 0.999f;
                if (fading) {
                    rlEnableColorBlend();
                    rlSetBlendMode(BLEND_ALPHA);
                    rlDisableDepthMask();
                }
                const bool drewMesh = DrawWorldDynamicModel(
                        *modelAsset,
                        model,
                        instance.model,
                        modelTransform,
                        dynamicModel.placedObjectId,
                        object.currentSectorId,
                        receiverBounds,
                        dynamicModel.containingSectorAmbient,
                        dynamicModel.environmentExposure,
                        lighting.vertical,
                        dynamicLightContext,
                        staticSpecularLights,
                        visibility,
                        objectProbeBakeCurrent,
                        environment,
                        true,
                        &instance,
                        &instance.meshNodeMatrices,
                        dynamicModel.emissiveScale,
                        dynamicModel.opacity,
                        entity == useHighlight.entity
                                ? useHighlight.strength
                                : 0.0f);
                if (fading) {
                    rlDisableColorBlend();
                    rlEnableDepthMask();
                }
                if (drewMesh) ++drawn;
                else ++skipped;
            });

    const auto drawModelDoor =
            [this,
             &assets,
             &dynamicLightContext,
             &staticSpecularLights,
             &visibility,
             environment,
             objectProbeBakeCurrent,
             &camera,
             viewWidth,
             viewHeight,
             viewAspect,
             viewNearPlane,
             &considered,
             &drawn,
             &portalCulled,
             &skipped,
             staticCaptureOnly](
                    engine::Entity,
                    SectorObject& object,
                    SectorObjectLighting& lighting,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render,
                    SectorDoorModelRender& modelRender) {
                if (!modelRender.modelVisualRequested) {
                    return;
                }
                ++considered;
                // Visibility and camera bounds were resolved once for both depth and color.
                if (!object.visible || !door.enabled || !render.visible) {
                    ++skipped;
                    return;
                }
                const engine::ModelAsset* leafAsset =
                        assets.GetModelAsset(modelRender.leafModel);
                const engine::ModelAsset* frameAsset =
                        assets.GetModelAsset(modelRender.frameModel);
                const SectorDoorModelDrawPolicy policy =
                        ResolveSectorDoorModelDrawPolicy(
                                modelRender,
                                leafAsset != nullptr,
                                frameAsset != nullptr);
                if (!policy.drawLeaf && !policy.drawFrame) {
                    ++skipped;
                    return;
                }

                const SectorReceiverBounds receiverBounds{
                        object.currentSectorId,
                        modelRender.receiverBounds.min,
                        modelRender.receiverBounds.max};
                bool drewDoorMesh = false;
                if (policy.drawLeaf) {
                    const Model& leaf = leafAsset->model;
                    drewDoorMesh = DrawWorldDynamicModel(
                            *leafAsset,
                            leaf,
                            modelRender.leafModel,
                            MatrixMultiply(leaf.transform, modelRender.leafMatrix),
                            door.placedObjectId,
                            object.currentSectorId,
                            receiverBounds,
                            modelRender.containingSectorAmbient,
                            modelRender.environmentExposure,
                            lighting.vertical,
                            dynamicLightContext,
                            staticSpecularLights,
                            visibility,
                            objectProbeBakeCurrent,
                            environment,
                            false) || drewDoorMesh;
                }
                if (policy.drawFrame) {
                    const Model& frame = frameAsset->model;
                    drewDoorMesh = DrawWorldDynamicModel(
                            *frameAsset,
                            frame,
                            modelRender.frameModel,
                            MatrixMultiply(frame.transform, modelRender.frameMatrix),
                            door.placedObjectId,
                            object.currentSectorId,
                            receiverBounds,
                            modelRender.containingSectorAmbient,
                            modelRender.environmentExposure,
                            lighting.vertical,
                            dynamicLightContext,
                            staticSpecularLights,
                            visibility,
                            objectProbeBakeCurrent,
                            environment,
                            false) || drewDoorMesh;
                }
                if (drewDoorMesh) ++drawn;
                else ++skipped;
            };
    drawingModelDoor = true;
    for (const auto& item : modelDoorDraws) {
        if (!runtimeObjectWorld.IsAlive(item.entity)) continue;
        drawModelDoor(item.entity, runtimeObjectWorld.Get<SectorObject>(item.entity),
                runtimeObjectWorld.Get<SectorObjectLighting>(item.entity),
                runtimeObjectWorld.Get<SectorDoor>(item.entity),
                runtimeObjectWorld.Get<SectorDoorResolvedAnchor>(item.entity),
                runtimeObjectWorld.Get<SectorDoorRender>(item.entity),
                runtimeObjectWorld.Get<SectorDoorModelRender>(item.entity));
    }
    drawingModelDoor = false;

    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    AppendStaticModelDebugText(
            renderDebugText,
            drawn,
            considered,
            portalCulled,
            skipped);
}

void SectorStaticModelRenderer::DrawViewmodel(
        engine::AssetManager& assets,
        const engine::ModelAsset& asset,
        engine::AnimatedModelInstance& instance,
        const Camera3D& camera,
        Matrix transform,
        const engine::ModelAsset* attachmentAsset,
        Matrix attachmentTransform,
        const SectorBillboardDynamicLightContext& dynamicLightContext,
        const SectorStaticSpecularLightContext& staticSpecularLights,
        bool objectProbeBakeCurrent,
        const TextureCubemap* environment,
        const BakedObjectLightingVerticalSample& ambientLighting,
        const SectorViewmodelLightingContext& lighting,
        const SectorViewmodelLightingContext& attachmentLighting)
{
    drawAssets=&assets;
    if (!shaderLoaded || !instance.poseReady || instance.poseFailed) return;
    const float opaque = 1.0f;
    if (modelOpacityLoc >= 0) {
        SetShaderValue(shader, modelOpacityLoc, &opaque, SHADER_UNIFORM_FLOAT);
    }
    UploadInteractionHighlightStrength(0.0f);

    SectorDynamicLightShaderLocations dynamicLocations;
    dynamicLocations.dynamicLightCount = dynamicLightCountLoc;
    dynamicLocations.dynamicLightPositions = dynamicLightPositionsLoc;
    dynamicLocations.dynamicLightColors = dynamicLightColorsLoc;
    dynamicLocations.dynamicLightRadii = dynamicLightRadiiLoc;
    dynamicLocations.dynamicLightIntensities = dynamicLightIntensitiesLoc;
    dynamicLocations.dynamicLightTypes = dynamicLightTypesLoc;
    dynamicLocations.dynamicLightDirections = dynamicLightDirectionsLoc;
    dynamicLocations.dynamicLightInnerConeCos = dynamicLightInnerConeCosLoc;
    dynamicLocations.dynamicLightOuterConeCos = dynamicLightOuterConeCosLoc;
    dynamicLocations.dynamicLightSpotShadowRight = dynamicLightSpotShadowRightLoc;
    dynamicLocations.dynamicLightSpotShadowProjection =
            dynamicLightSpotShadowProjectionLoc;
    dynamicLocations.dynamicLightProfiles = dynamicLightProfilesLoc;
    dynamicLocations.dynamicLightProfileParameters =
            dynamicLightProfileParametersLoc;
    dynamicLocations.flashlightCookie = flashlightCookieLoc;
    dynamicLocations.hasPointShadows = hasPointShadowsLoc;
    UploadSectorRendererDynamicPointLights(shader, dynamicLocations, dynamicLightContext);
    UploadSectorStaticSpecularLights(
            shader,
            staticSpecularLocations,
            staticSpecularLights);
    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    shadowLocations.shadowAtlasTilesPerRow = shadowAtlasTilesPerRowLoc;
    UploadSectorRendererDynamicSpotLightShadowUniforms(shader, shadowLocations, dynamicLightContext.shadowUniforms);
    UploadSectorFogShaderValues(shader, fogShaderLocations, SectorFogRenderContext{});
    if (cameraPositionLoc >= 0) SetShaderValue(shader, cameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
    const bool environmentActive = environment && environment->id != 0;
    viewmodelDiagnostics = {};

    const int disabled = 0;
    if (hasStaticLightmapLoc >= 0) SetShaderValue(shader, hasStaticLightmapLoc, &disabled, SHADER_UNIFORM_INT);
    if (useBakedAmbientOcclusionLoc >= 0) SetShaderValue(shader, useBakedAmbientOcclusionLoc, &disabled, SHADER_UNIFORM_INT);
    const Vector3 fallbackAmbient = SanitizeSectorPbrNonnegative(
            ambientLighting.lower.ambientCube[2]);
    if (containingSectorAmbientLoc >= 0) SetShaderValue(shader, containingSectorAmbientLoc, &fallbackAmbient, SHADER_UNIFORM_VEC3);
    for (size_t face = 0; face < objectAmbientCubeLocs.size(); ++face) {
        const Vector3 lowerAmbient = SanitizeSectorPbrNonnegative(
                ambientLighting.lower.ambientCube[face]);
        const Vector3 upperAmbient = SanitizeSectorPbrNonnegative(
                ambientLighting.upper.ambientCube[face]);
        if (objectAmbientCubeLocs[face] >= 0) SetShaderValue(shader, objectAmbientCubeLocs[face], &lowerAmbient, SHADER_UNIFORM_VEC3);
        if (objectAmbientCubeUpperLocs[face] >= 0) SetShaderValue(shader, objectAmbientCubeUpperLocs[face], &upperAmbient, SHADER_UNIFORM_VEC3);
    }
    const float lowerProbeHeight = std::isfinite(ambientLighting.lowerHeightWorld)
            ? ambientLighting.lowerHeightWorld : 0.0f;
    const float upperProbeHeight = std::isfinite(ambientLighting.upperHeightWorld)
            ? ambientLighting.upperHeightWorld : lowerProbeHeight;
    if (objectAmbientCubeLowerHeightLoc >= 0) SetShaderValue(shader, objectAmbientCubeLowerHeightLoc, &lowerProbeHeight, SHADER_UNIFORM_FLOAT);
    if (objectAmbientCubeUpperHeightLoc >= 0) SetShaderValue(shader, objectAmbientCubeUpperHeightLoc, &upperProbeHeight, SHADER_UNIFORM_FLOAT);
    rlDisableColorBlend();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
    const bool validProbe = ambientLighting.lower.valid
            || ambientLighting.upper.valid;
    const auto drawModel = [this,
                            environment,
                            environmentActive,
                            validProbe,
                            objectProbeBakeCurrent,
                            &staticSpecularLights,
                            &dynamicLightContext](
            const engine::ModelAsset& modelAsset,
            Model model,
            Matrix itemTransform,
            const SectorViewmodelLightingContext& itemLighting,
            SectorPbrLightingPath path) {
        const int skinningEnabled = 1;
        const int skinningDisabled = 0;
        const bool canSkin = model.skeleton.boneCount > 0
                && model.skeleton.boneCount <= engine::MaxAnimatedModelBones
                && model.boneMatrices != nullptr
                && shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] >= 0;
        if (useSkinningLoc >= 0) {
            SetShaderValue(
                    shader,
                    useSkinningLoc,
                    canSkin ? &skinningEnabled : &skinningDisabled,
                    SHADER_UNIFORM_INT);
        }
        if (canSkin) {
            rlEnableShader(shader.id);
            rlSetUniformMatrices(
                    shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
                    model.boneMatrices,
                    model.skeleton.boneCount);
        }

        const Matrix modelTransform = MatrixMultiply(
                model.transform, itemTransform);
        for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
            if (!model.meshMaterial) continue;
            const int materialIndex = model.meshMaterial[meshIndex];
            if (materialIndex < 0 || materialIndex >= model.materialCount) continue;
            const Material& source = model.materials[materialIndex];
            if (!source.maps) continue;
            std::array<MaterialMap, SectorStaticModelMaterialMapCount> maps{};
            std::copy_n(
                    source.maps,
                    SectorStaticModelMaterialMapCount,
                    maps.begin());
            ConfigureSectorStaticModelAuxiliaryMaterialMaps(
                    maps,
                    nullptr,
                    false,
                    environment,
                    dynamicLightContext.shadowMaps.shadowMap0,
                    dynamicLightContext.shadowMaps.shadowMap1);
            Material material = source;
            material.shader = shader;
            material.maps = maps.data();
            engine::ModelMaterialAsset pbr;
            if (materialIndex < static_cast<int>(modelAsset.materials.size())
                    && modelAsset.materials[
                            static_cast<size_t>(materialIndex)].pbrMetallicRoughness) {
                pbr = modelAsset.materials[static_cast<size_t>(materialIndex)];
            } else {
                pbr.baseColorFactor = ColorToNormalizedVector4(
                        maps[MATERIAL_MAP_DIFFUSE].color);
                pbr.roughnessFactor = 1.0f;
                pbr.hasBaseColorTexture =
                        maps[MATERIAL_MAP_DIFFUSE].texture.id != 0;
            }
            ApplySectorViewmodelMaterialOverride(
                    itemLighting,
                    pbr.metallicFactor,
                    pbr.roughnessFactor,
                    pbr.hasMetallicTexture,
                    pbr.hasRoughnessTexture);
            pbr = NormalizeSectorPbrMaterial(pbr);
            if (baseColorFactorLoc >= 0) SetShaderValue(shader, baseColorFactorLoc, &pbr.baseColorFactor, SHADER_UNIFORM_VEC4);
            if (emissiveFactorLoc >= 0) SetShaderValue(shader, emissiveFactorLoc, &pbr.emissiveFactor, SHADER_UNIFORM_VEC3);
            if (emissiveStrengthLoc >= 0) SetShaderValue(shader, emissiveStrengthLoc, &pbr.emissiveStrength, SHADER_UNIFORM_FLOAT);
            if (metallicFactorLoc >= 0) SetShaderValue(shader, metallicFactorLoc, &pbr.metallicFactor, SHADER_UNIFORM_FLOAT);
            if (roughnessFactorLoc >= 0) SetShaderValue(shader, roughnessFactorLoc, &pbr.roughnessFactor, SHADER_UNIFORM_FLOAT);
            if (normalScaleLoc >= 0) SetShaderValue(shader, normalScaleLoc, &pbr.normalScale, SHADER_UNIFORM_FLOAT);
            if (occlusionStrengthLoc >= 0) SetShaderValue(shader, occlusionStrengthLoc, &pbr.occlusionStrength, SHADER_UNIFORM_FLOAT);
            const int hasBase = pbr.hasBaseColorTexture;
            const int hasMetal = pbr.hasMetallicTexture;
            const int hasNormal = pbr.hasNormalTexture;
            const int hasRough = pbr.hasRoughnessTexture;
            const int hasOcclusion = pbr.hasOcclusionTexture;
            const int hasEmissive = pbr.hasEmissiveTexture;
            if (hasBaseColorTextureLoc >= 0) SetShaderValue(shader, hasBaseColorTextureLoc, &hasBase, SHADER_UNIFORM_INT);
            if (hasMetallicTextureLoc >= 0) SetShaderValue(shader, hasMetallicTextureLoc, &hasMetal, SHADER_UNIFORM_INT);
            if (hasNormalTextureLoc >= 0) SetShaderValue(shader, hasNormalTextureLoc, &hasNormal, SHADER_UNIFORM_INT);
            if (hasRoughnessTextureLoc >= 0) SetShaderValue(shader, hasRoughnessTextureLoc, &hasRough, SHADER_UNIFORM_INT);
            if (hasOcclusionTextureLoc >= 0) SetShaderValue(shader, hasOcclusionTextureLoc, &hasOcclusion, SHADER_UNIFORM_INT);
            if (hasEmissiveTextureLoc >= 0) SetShaderValue(shader, hasEmissiveTextureLoc, &hasEmissive, SHADER_UNIFORM_INT);
            const SectorPbrDrawState drawState = BuildSectorPbrDrawState(
                    path,
                    validProbe,
                    false,
                    objectProbeBakeCurrent,
                    environmentActive,
                    itemLighting.environmentExposure,
                    itemLighting.brightnessMultiplier,
                    itemLighting.materialOverrideEnabled,
                    contributionSettings);
            UploadPbrDrawState(drawState);
            UploadPbrMaterialTransferState(pbr);
            RecordPbrDiagnostics(
                    viewmodelDiagnostics,
                    -1,
                    engine::NullModelHandle(),
                    materialIndex,
                    drawState,
                    pbr,
                    staticSpecularLights);
            DrawMesh(model.meshes[meshIndex], material, modelTransform);
        }
    };

    if (attachmentAsset != nullptr) {
        drawModel(
                *attachmentAsset,
                attachmentAsset->model,
                attachmentTransform,
                attachmentLighting,
                SectorPbrLightingPath::ViewmodelAttachment);
    }
    drawModel(
            asset,
            engine::BuildAnimatedModelPoseView(asset, instance),
            transform,
            lighting,
            SectorPbrLightingPath::Viewmodel);
    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

} // namespace game
