#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"
#include "sector_demo/renderer/SectorReflectionProbePolicy.h"


#include "engine/assets/TextureLoadFlags.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMeshBuilder.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorSkyCylinder.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <external/glad.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace game {

namespace {

constexpr const char* kSectorDefaultMaterialTextureAssetPath =
        "assets/engine/default_material.png";

engine::TextureHandle CreatePlayerFlashlightCookie(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope)
{
    constexpr int Size = 256;
    Image image = GenImageColor(Size, Size, BLACK);
    for (int y = 0; y < Size; ++y) {
        for (int x = 0; x < Size; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f)
                    / static_cast<float>(Size) * 2.0f - 1.0f;
            const float ny = (static_cast<float>(y) + 0.5f)
                    / static_cast<float>(Size) * 2.0f - 1.0f;
            const float radius = std::sqrt(nx * nx + ny * ny);
            const float ring = 0.5f + 0.5f * std::cos(radius * 35.0f);
            const float patternT = std::clamp(
                    (1.0f - radius) / 0.35f, 0.0f, 1.0f);
            const float patternEnvelope = patternT * patternT
                    * (3.0f - 2.0f * patternT);
            const float asymmetry = nx * 0.06f - ny * 0.04f;
            // Keep the cookie neutral at its boundary. The shared flashlight
            // profile owns the only radial cutoff and feather.
            const float value = std::clamp(
                    0.5f
                            + (ring - 0.5f) * 0.5f * patternEnvelope
                            + asymmetry * patternEnvelope,
                    0.0f,
                    1.0f);
            const unsigned char channel = static_cast<unsigned char>(
                    std::lround(value * 255.0f));
            ImageDrawPixel(&image, x, y, Color{channel, channel, channel, 255});
        }
    }
    const engine::TextureHandle handle = assets.CreateTextureFromImage(
            scope,
            "player_flashlight_cookie",
            image,
            engine::TextureColorUsage::LinearData,
            engine::TextureLoad_BilinearFilter);
    UnloadImage(image);
    return handle;
}

constexpr float DefaultVisibilityDebugAspect = 16.0f / 9.0f;
constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;

bool BlitFramebufferColor(
        const RenderTexture2D& source,
        const RenderTexture2D& destination)
{
    if (source.id == 0 || destination.id == 0
            || source.texture.width != destination.texture.width
            || source.texture.height != destination.texture.height) {
        return false;
    }
    GLint previousReadFramebuffer = 0;
    GLint previousDrawFramebuffer = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
    while (glGetError() != GL_NO_ERROR) {}
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination.id);
    glBlitFramebuffer(
            0, 0, source.texture.width, source.texture.height,
            0, 0, destination.texture.width, destination.texture.height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
    const bool copied = glGetError() == GL_NO_ERROR;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
            static_cast<GLuint>(previousReadFramebuffer));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
            static_cast<GLuint>(previousDrawFramebuffer));
    return copied;
}

void AppendBillboardRenderDebugText(std::string& renderDebugText, const std::string& billboardText)
{
    const size_t existing = renderDebugText.find(" | billboards:");
    if (existing != std::string::npos) {
        renderDebugText.erase(existing);
    }
    if (!billboardText.empty() && !renderDebugText.empty()) {
        renderDebugText += " | " + billboardText;
    }
}

Vector2 PreviewYawForwardXZ(float yawRadians)
{
    return Vector2{std::cos(yawRadians), std::sin(yawRadians)};
}

float VisibilityDebugHorizontalFovRadians(const Camera3D& camera, float pitchRadians)
{
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float aspect = screenWidth > 0 && screenHeight > 0
            ? static_cast<float>(screenWidth) / static_cast<float>(screenHeight)
            : DefaultVisibilityDebugAspect;
    const float verticalFovRadians = camera.fovy * DegreesToRadians;
    return ComputeRuntimePortalVisibilityHorizontalFovRadians(
            verticalFovRadians,
            aspect,
            pitchRadians);
}

int GetShaderLocationArrayBase(Shader shader, const char* name)
{
    const int location = GetShaderLocation(shader, name);
    if (location >= 0) {
        return location;
    }

    const std::string indexedName = std::string(name) + "[0]";
    return GetShaderLocation(shader, indexedName.c_str());
}

int GetShaderLocationArrayElement(Shader shader, const char* name, std::size_t index)
{
    const std::string indexedName = std::string(name) + "[" + std::to_string(index) + "]";
    return GetShaderLocation(shader, indexedName.c_str());
}

void InitializeSectorSurfaceSamplerUnits(Shader shader)
{
    for (int textureUnit = MATERIAL_MAP_ALBEDO;
            textureUnit <= MATERIAL_MAP_CUBEMAP;
            ++textureUnit) {
        const int location = shader.locs[
                SHADER_LOC_MAP_DIFFUSE + textureUnit];
        if (location >= 0) {
            SetShaderValue(
                    shader,
                    location,
                    &textureUnit,
                    SHADER_UNIFORM_INT);
        }
    }
    rlDisableShader();
}

const char* DynamicLightDebugPrefix(SectorPreviewDynamicLightKind kind)
{
    return kind == SectorPreviewDynamicLightKind::Spot
            ? "spot:"
            : kind == SectorPreviewDynamicLightKind::Rect
                    ? "rect:"
                    : "point:";
}

std::string FormatDynamicLightDebugText(
        bool dynamicLightingEnabled,
        size_t selectedCount,
        size_t candidateCount,
        size_t totalCount,
        const std::vector<SectorPreviewDynamicLightKey>& selectedKeys)
{
    std::ostringstream out;
    out << "dynamic lights: selected "
            << selectedCount
            << " | portal eligible "
            << candidateCount
            << " | sources "
            << totalCount
            << " ("
            << (dynamicLightingEnabled ? "on" : "off")
            << ")";
    if (!selectedKeys.empty()) {
        out << " | selected: ";
        for (size_t i = 0; i < selectedKeys.size(); ++i) {
            if (i > 0) {
                out << ",";
            }
            out << DynamicLightDebugPrefix(selectedKeys[i].kind)
                    << selectedKeys[i].lightId;
        }
    }
    return out.str();
}

size_t CountDynamicSpotLightShadowCandidates(const std::vector<SectorPreviewDynamicPointLightUniform>& selectedLights)
{
    size_t count = 0;
    for (const SectorPreviewDynamicPointLightUniform& light : selectedLights) {
        if (light.castsShadow) {
            ++count;
        }
    }
    return count;
}

std::string FormatDynamicSpotLightShadowDebugText(
        size_t casterCount,
        size_t candidateCount,
        size_t maxCasterCount,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& casters,
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedLights)
{
    std::ostringstream out;
    out << "shadow casters: "
            << casterCount
            << " / "
            << candidateCount
            << " candidates / max "
            << maxCasterCount;
    if (!casters.empty()) {
        out << " | shadow ids: ";
        for (size_t i = 0; i < casters.size(); ++i) {
            if (i > 0) {
                out << ",";
            }
            const SectorPreviewDynamicSpotLightShadowCaster& caster = casters[i];
            const SectorPreviewDynamicLightKind kind = caster.dynamicLightIndex >= 0
                            && static_cast<std::size_t>(caster.dynamicLightIndex)
                                    < selectedLights.size()
                    ? selectedLights[static_cast<std::size_t>(
                            caster.dynamicLightIndex)].kind
                    : SectorPreviewDynamicLightKind::Spot;
            out << DynamicLightDebugPrefix(kind)
                    << caster.lightId;
        }
    }
    return out.str();
}

std::vector<std::string> SortedRendererMaterialIds(
        const SectorTopologyMap& map,
        const SectorGeneratedGeometry& geometry)
{
    std::unordered_set<std::string> unique;
    for (const SectorGeneratedSurface& surface : geometry.surfaces) {
        if (!surface.materialId.empty()) unique.insert(surface.materialId);
        if (!surface.decalMaterialId.empty()) unique.insert(surface.decalMaterialId);
    }
    if (ShouldRenderSkyCylinder(map) && !map.skySettings.materialId.empty()) {
        unique.insert(map.skySettings.materialId);
    }
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "door"
                && !object.door.materialId.empty()) {
            unique.insert(object.door.materialId);
        }
    }
    std::vector<std::string> ids(unique.begin(), unique.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::unordered_set<std::string> NormalMappedRendererMaterialIds(
        const SectorTopologyMap& map,
        const SectorGeneratedGeometry& geometry)
{
    std::unordered_set<std::string> ids;
    for (const SectorGeneratedSurface& surface : geometry.surfaces) {
        if (!surface.materialId.empty()) ids.insert(surface.materialId);
    }
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        if (object.kind == "door" && !object.door.materialId.empty()) {
            ids.insert(object.door.materialId);
        }
    }
    return ids;
}

bool LoadPreviewMaterial(
        Material& material,
        Texture2D& defaultMaterialTexture,
        bool& materialLoaded,
        int& useLightmapLoc,
        int& useBakedAmbientOcclusionLoc,
        int& hasLightmapLoc,
        int& hasDirectionalLightmapLoc,
        int& hasNormalMapLoc,
        int& normalStrengthLoc,
        int& materialPropertiesKindLoc,
        int& metallicFactorLoc,
        int& roughnessFactorLoc,
        int& cameraPositionLoc,
        int& hasEnvironmentLoc,
        int& environmentExposureLoc,
        int& indirectDiffuseScaleLoc,
        int& environmentSpecularScaleLoc,
        int& environmentBoxProjectionLoc,
        int& environmentCapturePositionLoc,
        int& environmentInfluenceCenterLoc,
        int& environmentHalfExtentsLoc,
        int& environmentYawLoc,
        int& environmentMaxLodLoc,
        int& environmentIntensityLoc,
        int& pbrDiagnosticModeLoc,
        int& useStaticSpecularLightingLoc,
        SectorStaticSpecularShaderLocations& staticSpecularLocations,
        int& alphaTestLoc,
        int& alphaCutoffLoc,
        int& hasDecalLoc,
        int& decalOpacityLoc,
        int& decalEmissiveLoc,
        int& decalEmissiveStrengthLoc,
        int& decalTintLoc,
        int& dynamicLightCountLoc,
        int& dynamicLightPositionsLoc,
        int& dynamicLightColorsLoc,
        int& dynamicLightRadiiLoc,
        int& dynamicLightIntensitiesLoc,
        int& dynamicLightTypesLoc,
        int& dynamicLightDirectionsLoc,
        int& dynamicLightInnerConeCosLoc,
        int& dynamicLightOuterConeCosLoc,
        int& dynamicLightSpotShadowRightLoc,
        int& dynamicLightSpotShadowProjectionLoc,
        int& dynamicLightProfilesLoc,
        int& dynamicLightProfileParametersLoc,
        int& flashlightCookieLoc,
        int& hasPointShadowsLoc,
        int& dynamicLightShadowSlotsLoc,
        std::array<int, MaxDynamicSpotLightShadowCasters>& shadowLightMatrixLocs,
        int& shadowBiasLoc,
        int& shadowStrengthLoc,
        int& shadowSoftnessLoc,
        int& shadowAtlasTilesPerRowLoc,
        SectorFogShaderLocations& fogShaderLocations,
        std::string& error)
{
    material = LoadMaterialDefault();
    Shader shader = LoadGameShader(GameShader::Lightmap);
    if (shader.id == 0) {
        UnloadMaterial(material);
        material = Material{};
        error = "Preview failed: could not load sector lightmap shader";
        return false;
    }
    material.shader = shader;
    material.shader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocationAttrib(material.shader, "vertexNormal");
    material.shader.locs[SHADER_LOC_VERTEX_TANGENT] = GetShaderLocationAttrib(material.shader, "vertexTangent");
    material.shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(material.shader, "texture0");
    material.shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(material.shader, "texture1");
    material.shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(material.shader, "decalTexture");
    material.shader.locs[SHADER_LOC_MAP_HEIGHT] = GetShaderLocation(material.shader, "normalTexture");
    material.shader.locs[SHADER_LOC_MAP_BRDF] =
            GetShaderLocation(material.shader, "materialPropertiesTexture");
    material.shader.locs[SHADER_LOC_MAP_EMISSION] =
            GetShaderLocation(material.shader, "directionalLightmapTexture");
    material.shader.locs[SHADER_LOC_MAP_CUBEMAP] =
            GetShaderLocation(material.shader, "environmentTexture");
    material.shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(material.shader, "shadowMap0");
    material.shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(material.shader, "shadowMap1");
    useLightmapLoc = GetShaderLocation(material.shader, "useLightmap");
    useBakedAmbientOcclusionLoc = GetShaderLocation(material.shader, "useBakedAmbientOcclusion");
    hasLightmapLoc = GetShaderLocation(material.shader, "hasLightmap");
    hasDirectionalLightmapLoc = GetShaderLocation(
            material.shader, "hasDirectionalLightmap");
    hasNormalMapLoc = GetShaderLocation(material.shader, "hasNormalMap");
    normalStrengthLoc = GetShaderLocation(material.shader, "normalStrength");
    materialPropertiesKindLoc = GetShaderLocation(
            material.shader, "materialPropertiesKind");
    metallicFactorLoc = GetShaderLocation(material.shader, "metallicFactor");
    roughnessFactorLoc = GetShaderLocation(material.shader, "roughnessFactor");
    cameraPositionLoc = GetShaderLocation(material.shader, "cameraPosition");
    hasEnvironmentLoc = GetShaderLocation(material.shader, "hasEnvironment");
    environmentExposureLoc = GetShaderLocation(
            material.shader, "environmentExposure");
    indirectDiffuseScaleLoc = GetShaderLocation(
            material.shader, "indirectDiffuseScale");
    environmentSpecularScaleLoc = GetShaderLocation(
            material.shader, "environmentSpecularScale");
    environmentBoxProjectionLoc = GetShaderLocation(material.shader, "environmentBoxProjection");
    environmentCapturePositionLoc = GetShaderLocation(material.shader, "environmentCapturePosition");
    environmentInfluenceCenterLoc = GetShaderLocation(material.shader, "environmentInfluenceCenter");
    environmentHalfExtentsLoc = GetShaderLocation(material.shader, "environmentHalfExtents");
    environmentYawLoc = GetShaderLocation(material.shader, "environmentYaw");
    environmentMaxLodLoc = GetShaderLocation(material.shader, "environmentMaxLod");
    environmentIntensityLoc = GetShaderLocation(material.shader, "environmentIntensity");
    pbrDiagnosticModeLoc = GetShaderLocation(
            material.shader, "pbrDiagnosticMode");
    useStaticSpecularLightingLoc = GetShaderLocation(
            material.shader, "useStaticSpecularLighting");
    staticSpecularLocations = GetSectorStaticSpecularShaderLocations(
            material.shader);
    alphaTestLoc = GetShaderLocation(material.shader, "alphaTest");
    alphaCutoffLoc = GetShaderLocation(material.shader, "alphaCutoff");
    hasDecalLoc = GetShaderLocation(material.shader, "hasDecal");
    decalOpacityLoc = GetShaderLocation(material.shader, "decalOpacity");
    decalEmissiveLoc = GetShaderLocation(material.shader, "decalEmissive");
    decalEmissiveStrengthLoc = GetShaderLocation(material.shader, "decalEmissiveStrength");
    decalTintLoc = GetShaderLocation(material.shader, "decalTint");
    dynamicLightCountLoc = GetShaderLocation(material.shader, "dynamicLightCount");
    dynamicLightPositionsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightPositions");
    dynamicLightColorsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightColors");
    dynamicLightRadiiLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightRadii");
    dynamicLightIntensitiesLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightIntensities");
    dynamicLightTypesLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightTypes");
    dynamicLightDirectionsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightDirections");
    dynamicLightInnerConeCosLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightInnerConeCos");
    dynamicLightOuterConeCosLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightOuterConeCos");
    dynamicLightSpotShadowRightLoc = GetShaderLocationArrayBase(
            material.shader, "dynamicLightSpotShadowRight");
    dynamicLightSpotShadowProjectionLoc = GetShaderLocationArrayBase(
            material.shader, "dynamicLightSpotShadowProjection");
    dynamicLightProfilesLoc = GetShaderLocationArrayBase(
            material.shader, "dynamicLightProfiles");
    dynamicLightProfileParametersLoc = GetShaderLocationArrayBase(
            material.shader, "dynamicLightProfileParameters");
    flashlightCookieLoc = GetShaderLocation(
            material.shader, "flashlightCookie");
    hasPointShadowsLoc = GetShaderLocation(material.shader, "hasPointShadows");
    dynamicLightShadowSlotsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        shadowLightMatrixLocs[i] = GetShaderLocationArrayElement(material.shader, "shadowLightMatrices", i);
    }
    shadowBiasLoc = GetShaderLocationArrayBase(material.shader, "shadowBias");
    shadowStrengthLoc = GetShaderLocationArrayBase(material.shader, "shadowStrength");
    shadowSoftnessLoc = GetShaderLocationArrayBase(material.shader, "shadowSoftness");
    shadowAtlasTilesPerRowLoc = GetShaderLocation(material.shader, "shadowAtlasTilesPerRow");
    fogShaderLocations = GetSectorFogShaderLocations(material.shader);
    InitializeSectorSurfaceSamplerUnits(material.shader);
    defaultMaterialTexture = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    materialLoaded = true;
    return true;
}

bool ComputeGeometryBounds(const SectorGeneratedGeometry& geometry, Vector3& outMin, Vector3& outMax)
{
    SectorAabb3 bounds = EmptySectorAabb3();
    bool found = false;
    for (const SectorGeneratedSurface& surface : geometry.surfaces) {
        for (const SectorGeneratedVertex& vertex : surface.vertices) {
            ExpandSectorAabb3(bounds, vertex.position);
            found = true;
        }
    }
    outMin = bounds.min;
    outMax = bounds.max;
    return found;
}

const SectorReceiverBounds* FindSectorReceiverBounds(
        const std::vector<SectorReceiverBounds>& bounds,
        int sectorId)
{
    const auto found = std::find_if(
            bounds.begin(),
            bounds.end(),
            [sectorId](const SectorReceiverBounds& candidate) {
                return candidate.sectorId == sectorId;
            });
    return found == bounds.end() ? nullptr : &*found;
}

} // namespace

bool SectorMeshRenderer::Rebuild(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const char* scopeName,
        std::string& error)
{
    return RebuildRendererResources(assets, map, scopeName, error);
}

void SectorMeshRenderer::EnsureSurfaceMaterialResources(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const SectorGeneratedGeometry& geometry)
{
    if (engine::IsNull(assetScope)) return;

    const std::unordered_set<std::string> normalMappedMaterialIds =
            NormalMappedRendererMaterialIds(map, geometry);
    for (const std::string& materialId :
            SortedRendererMaterialIds(map, geometry)) {
        const auto it = map.resolvedMaterialsById.find(materialId);
        if (it == map.resolvedMaterialsById.end()) {
            std::fprintf(
                    stderr,
                    "[SectorMeshRenderer WARNING] Missing global material '%s'; using fallback texture\n",
                    materialId.c_str());
            continue;
        }

        const SectorMaterialDefinition& texture = it->second;
        const std::string resolvedPath = ResolveSectorAssetPath(texture.path);
        const std::string albedoRequestKey = resolvedPath + "|srgb|"
                + std::to_string(static_cast<int>(texture.filter));
        const engine::TextureHandle handle = assets.RequestTexture(
                assetScope,
                albedoRequestKey.c_str(),
                resolvedPath.c_str(),
                engine::TextureColorUsage::SceneSrgb,
                SectorMaterialTextureLoadFlags(texture.filter));
        textureHandlesById.insert_or_assign(texture.id, handle);
        metallicFactorById.insert_or_assign(
                texture.id, texture.metallicFactor);
        roughnessFactorById.insert_or_assign(
                texture.id, texture.roughnessFactor);

        if (normalMappedMaterialIds.find(materialId)
                == normalMappedMaterialIds.end()) {
            normalTextureHandlesById.erase(texture.id);
            normalStrengthById.erase(texture.id);
            propertyTextureHandlesById.erase(texture.id);
            propertyMapKindsById.erase(texture.id);
            continue;
        }
        normalStrengthById.insert_or_assign(
                texture.id, texture.normalStrength);

        const std::string normalMapPath =
                SectorMaterialNormalMapPath(texture.path);
        const std::string resolvedNormalMapPath =
                ResolveSectorAssetPath(normalMapPath);
        std::error_code normalMapError;
        if (normalMapPath.empty()
                || !std::filesystem::is_regular_file(
                        resolvedNormalMapPath, normalMapError)
                || normalMapError) {
            normalTextureHandlesById.erase(texture.id);
        } else {
            const std::string normalMapKey = resolvedNormalMapPath + "|linear|"
                    + std::to_string(static_cast<int>(texture.filter));
            normalTextureHandlesById.insert_or_assign(
                    texture.id,
                    assets.RequestTexture(
                            assetScope,
                            normalMapKey.c_str(),
                            resolvedNormalMapPath.c_str(),
                            engine::TextureColorUsage::LinearData,
                            SectorMaterialTextureLoadFlags(texture.filter)));
        }

        SectorMaterialPropertyMapKind propertyMapKind =
                SectorMaterialPropertyMapKind::None;
        std::string propertyMapPath = SectorMaterialOrmMapPath(texture.path);
        std::string resolvedPropertyMapPath = ResolveSectorAssetPath(propertyMapPath);
        std::error_code propertyMapError;
        if (!propertyMapPath.empty()
                && std::filesystem::is_regular_file(
                        resolvedPropertyMapPath, propertyMapError)
                && !propertyMapError) {
            propertyMapKind = SectorMaterialPropertyMapKind::Orm;
        } else {
            propertyMapError.clear();
            propertyMapPath = SectorMaterialRoughnessMapPath(texture.path);
            resolvedPropertyMapPath = ResolveSectorAssetPath(propertyMapPath);
            if (!propertyMapPath.empty()
                    && std::filesystem::is_regular_file(
                            resolvedPropertyMapPath, propertyMapError)
                    && !propertyMapError) {
                propertyMapKind = SectorMaterialPropertyMapKind::Roughness;
            }
        }
        if (propertyMapKind == SectorMaterialPropertyMapKind::None) {
            propertyTextureHandlesById.erase(texture.id);
            propertyMapKindsById.erase(texture.id);
            continue;
        }
        const std::string propertyMapKey = resolvedPropertyMapPath + "|linear|"
                + std::to_string(static_cast<int>(texture.filter));
        propertyTextureHandlesById.insert_or_assign(
                texture.id,
                assets.RequestTexture(
                        assetScope,
                        propertyMapKey.c_str(),
                        resolvedPropertyMapPath.c_str(),
                        engine::TextureColorUsage::LinearData,
                        SectorMaterialTextureLoadFlags(texture.filter)));
        propertyMapKindsById.insert_or_assign(texture.id, propertyMapKind);
    }
}

bool SectorMeshRenderer::RefreshSurfaceMaterials(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        std::string& error)
{
    return RefreshSurfaceGeometryInternal(assets, map, false, error);
}

bool SectorMeshRenderer::RefreshSurfaceGeometry(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        std::string& error)
{
    return RefreshSurfaceGeometryInternal(assets, map, true, error);
}

bool SectorMeshRenderer::RefreshSurfaceGeometryInternal(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        bool refreshVisibilityData,
        std::string& error)
{
    error.clear();
    if (!initialized || engine::IsNull(assetScope)) {
        error = "Preview surface refresh requires an initialized renderer";
        return false;
    }
    if (map.sectors.empty()) {
        error = "Preview surface refresh requires topology sectors";
        return false;
    }

    SectorGeneratedGeometry candidateGeometry;
    if (!BuildSectorGeneratedGeometry(map, candidateGeometry, &error)) {
        if (error.empty()) {
            error = "Topology generated no surface geometry";
        }
        return false;
    }

    EnsureSurfaceMaterialResources(assets, map, candidateGeometry);

    SectorLightmapLayout lightmapLayout;
    const std::string currentSurfaceHash =
            ComputeSectorLightmapSourceHash(map);
    const SectorLightmapStatus currentLightmapStatus =
            GetSectorLightmapStatus(map, currentSurfaceHash);
    std::string layoutError;
    const bool useLightmapLayout =
            currentLightmapStatus == SectorLightmapStatus::Valid
            && !lightmapTextures.empty()
            && lightmapTextures.size() == directionalLightmapTextures.size()
            && BuildSectorLightmapLayout(map, lightmapLayout, layoutError)
            && lightmapLayout.atlasCount
                    <= static_cast<int>(lightmapTextures.size());

    SectorMeshBuildResult candidateMeshes =
            BuildSectorMeshesFromGeneratedGeometry(
                    candidateGeometry,
                    useLightmapLayout ? &lightmapLayout : nullptr,
                    &error);
    if (candidateMeshes.sectorDrawRecords.empty()) {
        if (error.empty()) {
            error = "Topology mesh builder produced no sector draw records";
        }
        return false;
    }

    RuntimeSectorVisibilityGraph candidateVisibilityGraph;
    SectorCollisionWorld candidateVisibilityLookupWorld;
    bool candidateVisibilityGraphValid = visibilityGraphValid;
    bool candidateVisibilityLookupWorldValid = visibilityLookupWorldValid;
    if (refreshVisibilityData) {
        std::string visibilityError;
        candidateVisibilityGraphValid = BuildRuntimeSectorVisibilityGraph(
                map, candidateVisibilityGraph, &visibilityError);
        if (!candidateVisibilityGraphValid) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Visibility graph refresh failed: %s\n",
                    visibilityError.c_str());
            candidateVisibilityGraph = {};
        }
        visibilityError.clear();
        candidateVisibilityLookupWorldValid =
                candidateVisibilityLookupWorld.BuildFromTopology(
                        map, &visibilityError);
        if (!candidateVisibilityLookupWorldValid) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Visibility sector lookup refresh failed: %s\n",
                    visibilityError.c_str());
        }
    }

    if (!liquidRenderer.Rebuild(map, candidateGeometry, error)) {
        return false;
    }
    UnloadSectorMeshes(meshes);
    meshes = std::move(candidateMeshes);
    visibleSectorDraws.reserve(meshes.sectorDrawRecords.size());
    generatedGeometry = std::move(candidateGeometry);
    sectorCount = map.sectors.size();
    if (refreshVisibilityData) {
        visibilityGraph = std::move(candidateVisibilityGraph);
        ReserveRuntimePortalVisibilityScratch(visibilityGraph, visibilityScratch);
        pbrEnvironment.portals = visibilityGraph.portals;
        visibilityGraphValid = candidateVisibilityGraphValid;
        visibilityLookupWorld = std::move(candidateVisibilityLookupWorld);
        visibilityLookupWorldValid = candidateVisibilityLookupWorldValid;
        dynamicLightState.RebuildSources(
                map,
                visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
        BuildSectorLightAtmosphereSources(
                map,
                visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
                lightAtmosphereSources);
        analyticFogRenderer.Reserve(map.compiledLocalFogVolumes.size());
        analyticLightShaftRenderer.Reserve(lightAtmosphereSources.size());
        lightProxyRenderer.Reserve(lightAtmosphereSources.size());
    }

    RefreshBakedDataStatus(map, currentSurfaceHash);
    surfaceLightmapBakeCurrent = surfaceLightmapBakeCurrent
            && useLightmapLayout;
    runtimeReflections.Invalidate(pbrEnvironment);
    if (staticObjectAdjustmentBakedDataActive) {
        surfaceLightmapBakeCurrent = false;
        objectProbeBakeCurrent = false;
    }

    RebuildSectorStaticSpecularLights(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            meshes.sectorReceiverBounds,
            staticSpecularLightState);
    dynamicLightState.ReserveReceiverBoundsCapacity(
            meshes.sectorReceiverBounds.size(),
            std::max(kSectorRuntimeObjectInitialCapacity,
                    map.runtimeObjects.size()));
    UpdateVisibilityDebug();
    return true;
}

bool SectorMeshRenderer::RebuildRendererResources(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const char* scopeName,
        std::string& error)
{
    Shutdown(assets);
    error.clear();

    if (map.sectors.empty()) {
        error = "Preview failed: topology map has no sectors";
        return false;
    }

    if (!BuildSectorGeneratedGeometry(map, generatedGeometry, &error)) {
        error = error.empty()
                ? "Preview failed: topology generated no geometry"
                : "Preview failed: " + error;
        generatedGeometry = {};
        return false;
    }
    billboardRenderer.ResetDebugState();
    staticModelRenderer.ResetDebugState();

    std::string visibilityError;
    visibilityGraphValid = BuildRuntimeSectorVisibilityGraph(map, visibilityGraph, &visibilityError);
    ReserveRuntimePortalVisibilityScratch(visibilityGraph, visibilityScratch);
    if (!visibilityGraphValid) {
        std::fprintf(stderr, "[SectorDemo WARNING] Visibility graph build failed: %s\n", visibilityError.c_str());
        visibilityGraph = {};
    }
    visibilityLookupWorldValid = visibilityLookupWorld.BuildFromTopology(map, &visibilityError);
    if (!visibilityLookupWorldValid) {
        std::fprintf(stderr, "[SectorDemo WARNING] Visibility sector lookup build failed: %s\n", visibilityError.c_str());
    }

    assetScope = assets.CreateScope(scopeName == nullptr ? "sector_mesh_preview" : scopeName);
    if (engine::IsNull(assetScope)) {
        generatedGeometry = {};
        error = "Preview failed: could not create asset scope";
        return false;
    }
    const std::string defaultMaterialTexturePath =
            ResolveSectorAssetPath(kSectorDefaultMaterialTextureAssetPath);
    defaultMaterialTextureHandle = assets.RequestTexture(
            assetScope,
            "sector_builtin_default_material",
            defaultMaterialTexturePath.c_str(),
            engine::TextureColorUsage::SceneSrgb,
            SectorMaterialTextureLoadFlags(SectorMaterialFilter::Anisotropic8x));
    flashlightCookieTexture = CreatePlayerFlashlightCookie(
            assets, assetScope);
    EnsureSurfaceMaterialResources(assets, map, generatedGeometry);

    if (ShouldRenderSkyCylinder(map)) {
        const SectorMaterialDefinition* skyTexture = FindSkyTexture(map);
        const engine::TextureHandle skyTextureHandle = skyTexture == nullptr
                ? engine::NullTextureHandle()
                : TextureForId(skyTexture->id);
        skyRenderer.Rebuild(map, skyTextureHandle);
    }
    BuildSectorPbrEnvironment(
            assets,
            assetScope,
            map,
            pbrEnvironment);

    SectorLightmapLayout lightmapLayout;
    const std::vector<SectorLightmapAtlasMetadata> lightmapAtlases =
            GetSectorLightmapAtlases(map.bakedLightmap);
    const SectorLightmapStatus status = GetSectorLightmapStatus(map);
    lightmapStatus = static_cast<int>(status);
    objectProbeBakeCurrent =
            GetSectorBakedObjectLightProbeStatus(map)
                    == SectorLightmapStatus::Valid;
    bool useLightmapLayout = status == SectorLightmapStatus::Valid
            && BuildSectorLightmapLayout(map, lightmapLayout, error);
    if (useLightmapLayout
            && lightmapLayout.atlasCount
                    > static_cast<int>(lightmapAtlases.size())) {
        error = "Baked lightmap metadata does not contain every topology atlas";
        useLightmapLayout = false;
        lightmapStatus = static_cast<int>(SectorLightmapStatus::Stale);
    }
    if (status == SectorLightmapStatus::Valid && !useLightmapLayout) {
        std::fprintf(stderr, "[SectorDemo WARNING] %s\n", error.c_str());
        error.clear();
    }

    SectorStaticModelLightmapData staticModelLightmapData;
    if (useLightmapLayout && HasAssignedSectorStaticModels(map)) {
        std::string staticModelError;
        if (!ReadSectorStaticModelLightmapSidecar(
                    ResolveSectorAssetPath(
                            map.bakedLightmap.staticModels.path),
                    &map.bakedLightmap.staticModels,
                    staticModelLightmapData,
                    staticModelError)) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Static model lightmap disabled: %s\n",
                    staticModelError.c_str());
            staticModelLightmapData = {};
        } else if (!AreSectorStaticModelLightmapAtlasIndicesValid(
                           staticModelLightmapData,
                           static_cast<int>(lightmapAtlases.size()))) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Static model lightmap disabled: atlas index is outside installed metadata\n");
            staticModelLightmapData = {};
        }
    }
    staticModelRenderer.SetLightmapData(
            std::move(staticModelLightmapData));

    if (useLightmapLayout) {
        lightmapTextures.reserve(lightmapAtlases.size());
        directionalLightmapTextures.reserve(lightmapAtlases.size());
        for (size_t atlasIndex = 0; atlasIndex < lightmapAtlases.size(); ++atlasIndex) {
            const std::string resolvedPath = ResolveSectorAssetPath(
                    lightmapAtlases[atlasIndex].path);
            const std::string key = "sector_lightmap_atlas_"
                    + std::to_string(atlasIndex);
            SectorLightmapArtifactData artifact;
            std::string artifactError;
            if (!ReadSectorLightmapArtifact(
                        resolvedPath,
                        &map.bakedLightmap,
                        artifact,
                        artifactError)) {
                std::fprintf(stderr,
                        "[SectorDemo WARNING] HDR lightmap disabled: %s\n",
                        artifactError.c_str());
                lightmapTextures.clear();
                directionalLightmapTextures.clear();
                useLightmapLayout = false;
                lightmapStatus = static_cast<int>(SectorLightmapStatus::Invalid);
                break;
            }
            Image image{};
            image.data = artifact.rgba16.data();
            image.width = artifact.width;
            image.height = artifact.height;
            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
            const engine::TextureHandle texture = assets.CreateTextureFromImage(
                    assetScope,
                    key.c_str(),
                    image,
                    engine::TextureColorUsage::LinearData,
                    engine::TextureLoad_BilinearFilter);
            if (engine::IsNull(texture)) {
                std::fprintf(stderr,
                        "[SectorDemo WARNING] HDR lightmap GPU upload failed for '%s'\n",
                        resolvedPath.c_str());
                lightmapTextures.clear();
                directionalLightmapTextures.clear();
                useLightmapLayout = false;
                lightmapStatus = static_cast<int>(SectorLightmapStatus::Invalid);
                break;
            }
            Image directionalImage{};
            directionalImage.data = artifact.directionalRgba8.data();
            directionalImage.width = artifact.width;
            directionalImage.height = artifact.height;
            directionalImage.mipmaps = 1;
            directionalImage.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            const std::string directionalKey =
                    "sector_directional_lightmap_atlas_"
                    + std::to_string(atlasIndex);
            const engine::TextureHandle directionalTexture =
                    assets.CreateTextureFromImage(
                            assetScope,
                            directionalKey.c_str(),
                            directionalImage,
                            engine::TextureColorUsage::LinearData,
                            engine::TextureLoad_BilinearFilter);
            if (engine::IsNull(directionalTexture)) {
                std::fprintf(stderr,
                        "[SectorDemo WARNING] Directional lightmap GPU upload failed for '%s'\n",
                        resolvedPath.c_str());
                lightmapTextures.clear();
                directionalLightmapTextures.clear();
                useLightmapLayout = false;
                lightmapStatus = static_cast<int>(SectorLightmapStatus::Invalid);
                break;
            }
            lightmapTextures.push_back(texture);
            directionalLightmapTextures.push_back(directionalTexture);
        }
    }

    std::string meshError;
    meshes = BuildSectorMeshes(map, useLightmapLayout ? &lightmapLayout : nullptr, &meshError);
    visibleSectorDraws.reserve(meshes.sectorDrawRecords.size());
    if (meshes.sectorDrawRecords.empty()) {
        Shutdown(assets);
        error = meshError.empty()
                ? "Preview failed: topology mesh builder produced no sector draw records"
                : "Preview failed: " + meshError;
        return false;
    }

    surfaceLightmapBakeCurrent = useLightmapLayout
            && static_cast<SectorLightmapStatus>(lightmapStatus)
                    == SectorLightmapStatus::Valid;
    RebuildSectorStaticSpecularLights(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            meshes.sectorReceiverBounds,
            staticSpecularLightState);

    runtimeReflections.Invalidate(pbrEnvironment);
    dynamicLightState.RebuildSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
    const size_t runtimeObjectCapacity = std::max(
            kSectorRuntimeObjectInitialCapacity,
            map.runtimeObjects.size());
    staticModelRenderer.ReserveShadowCasterCapacity(runtimeObjectCapacity);
    dynamicModelShadowRenderer.ReserveShadowCasterCapacity(
            runtimeObjectCapacity);
    dynamicLightState.ReserveReceiverBoundsCapacity(
            meshes.sectorReceiverBounds.size(),
            runtimeObjectCapacity);
    BuildSectorLightAtmosphereSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            lightAtmosphereSources);
    doorRenderer.ReserveRuntimeDoorCapacity(runtimeObjectCapacity);
    windowRenderer.Reserve(runtimeObjectCapacity);
    runtimeSeconds = 0.0f;
    distanceFogRenderer.Shutdown();
    analyticFogRenderer.Shutdown();
    analyticLightShaftRenderer.Shutdown();
    lightProxyRenderer.Shutdown();
    lightDustRenderer.Shutdown();
    if (!underwaterRenderer.Initialize(assets, assetScope)) {
        TraceLog(LOG_WARNING,
                "UNDERWATER: optional visual resources are incomplete; unavailable effects disabled");
    }
    analyticFogRenderer.Reserve(map.compiledLocalFogVolumes.size());
    analyticLightShaftRenderer.Reserve(lightAtmosphereSources.size());
    lightProxyRenderer.Reserve(lightAtmosphereSources.size());
    UnloadHdrSceneColorView();

    if (!dynamicLightState.EnsureShadowMapResources()) {
        Shutdown(assets);
        error = "Preview failed: could not create dynamic spotlight shadow maps";
        return false;
    }

    if (!dynamicLightState.LoadShadowMaterial()) {
        Shutdown(assets);
        error = "Preview failed: could not load dynamic spotlight shadow shader";
        return false;
    }

    if (!dynamicModelShadowRenderer.Load()) {
        Shutdown(assets);
        error = "Preview failed: could not load dynamic model shadow renderer";
        return false;
    }
    if (!billboardRenderer.Load()) {
        Shutdown(assets);
        error = "Preview failed: could not load billboard cutout shader";
        return false;
    }

    if (!staticModelRenderer.Load()) {
        Shutdown(assets);
        error = "Preview failed: could not load static model shader";
        return false;
    }

    if (!doorRenderer.LoadOpaqueResources()) {
        Shutdown(assets);
        error = "Preview failed: could not load door opaque shader";
        return false;
    }

    worldProfiler.Initialize(8 + runtimeObjectCapacity * 2);
    if (!windowRenderer.Initialize(runtimeObjectCapacity)) {
        Shutdown(assets);
        error = "Preview failed: could not load window transparency shader";
        return false;
    }
    if (!ductCoverRenderer.Initialize()) {
        error = "Failed to initialize Duct Access cover renderer";
        Shutdown(assets);
        return false;
    }
    if (!liquidRenderer.Initialize(map.sectors.size())) {
        Shutdown(assets);
        error = "Preview failed: could not load liquid transparency shader";
        return false;
    }
    if (!liquidRenderer.Rebuild(map, generatedGeometry, error)) {
        Shutdown(assets);
        error = error.empty()
                ? "Preview failed: could not build liquid surfaces"
                : "Preview failed: " + error;
        return false;
    }

    if (!LoadPreviewMaterial(
                material,
                defaultMaterialTexture,
                materialLoaded,
                useLightmapLoc,
                useBakedAmbientOcclusionLoc,
                hasLightmapLoc,
                hasDirectionalLightmapLoc,
                hasNormalMapLoc,
                normalStrengthLoc,
                materialPropertiesKindLoc,
                metallicFactorLoc,
                roughnessFactorLoc,
                cameraPositionLoc,
                hasEnvironmentLoc,
                environmentExposureLoc,
                indirectDiffuseScaleLoc,
                environmentSpecularScaleLoc,
                environmentBoxProjectionLoc,
                environmentCapturePositionLoc,
                environmentInfluenceCenterLoc,
                environmentHalfExtentsLoc,
                environmentYawLoc,
                environmentMaxLodLoc,
                environmentIntensityLoc,
                pbrDiagnosticModeLoc,
                useStaticSpecularLightingLoc,
                staticSpecularLocations,
                alphaTestLoc,
                alphaCutoffLoc,
                hasDecalLoc,
                decalOpacityLoc,
                decalEmissiveLoc,
                decalEmissiveStrengthLoc,
                decalTintLoc,
                dynamicLightCountLoc,
                dynamicLightPositionsLoc,
                dynamicLightColorsLoc,
                dynamicLightRadiiLoc,
                dynamicLightIntensitiesLoc,
                dynamicLightTypesLoc,
                dynamicLightDirectionsLoc,
                dynamicLightInnerConeCosLoc,
                dynamicLightOuterConeCosLoc,
                dynamicLightSpotShadowRightLoc,
                dynamicLightSpotShadowProjectionLoc,
                dynamicLightProfilesLoc,
                dynamicLightProfileParametersLoc,
                flashlightCookieLoc,
                hasPointShadowsLoc,
                dynamicLightShadowSlotsLoc,
                shadowLightMatrixLocs,
                shadowBiasLoc,
                shadowStrengthLoc,
                shadowSoftnessLoc,
                shadowAtlasTilesPerRowLoc,
                fogShaderLocations,
                error)) {
        Shutdown(assets);
        return false;
    }

    reflectionLocations = LoadSectorReflectionShaderLocations(material.shader);
    const std::size_t reflectionLightCapacity = map.dynamicPointLights.size()
            + map.dynamicSpotLights.size() + map.dynamicRectLights.size();
    runtimeReflections.Initialize(assets, assetScope, pbrEnvironment, reflectionLightCapacity,
            map.sectors.size(), runtimeObjectCapacity);
    reflectionCaptureAssets = &assets;
    depthPrepassMaterial = LoadMaterialDefault();
    Shader depthShader = LoadGameShader(GameShader::DepthPrepass);
    if (depthShader.id == 0) {
        UnloadMaterial(depthPrepassMaterial);
        depthPrepassMaterial = {};
        Shutdown(assets);
        error = "Preview failed: could not load depth pre-pass shader";
        return false;
    }
    depthPrepassMaterial.shader = depthShader;
    depthPrepassMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(depthShader, "vertexPosition");
    depthPrepassMaterial.shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(depthShader, "mvp");
    depthPrepassMaterialLoaded = true;

    sectorCount = map.sectors.size();

    Vector3 boundsMin{};
    Vector3 boundsMax{};
    if (ComputeGeometryBounds(generatedGeometry, boundsMin, boundsMax)) {
        const Vector3 center = Vector3Scale(Vector3Add(boundsMin, boundsMax), 0.5f);
        const float height = std::max(1.6f, (boundsMax.y - boundsMin.y) * 0.5f);
        position = Vector3{center.x, boundsMin.y + height, center.z};
    } else {
        position = Vector3{0.0f, 1.6f, 0.0f};
    }
    yawRadians = 0.0f;
    pitchRadians = 0.0f;
    camera.fovy = verticalFovDegrees;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateCamera();
    UpdateVisibilityDebug();

    distanceFogRenderer.Initialize();
    analyticFogRenderer.Initialize();
    analyticLightShaftRenderer.Initialize();
    lightProxyRenderer.Initialize();
    lightDustRenderer.Initialize();
    bloomRenderer.Initialize();
    InitializeHdrCompositeShader();

    initialized = true;
    return true;
}

void SectorMeshRenderer::Shutdown(engine::AssetManager& assets)
{
    ShutdownRendererResources(assets);
}

void SectorMeshRenderer::ShutdownRendererResources(engine::AssetManager& assets)
{
    worldProfiler.Shutdown();
    worldDiagnosticsEnabled = false;
    runtimeReflections.Shutdown();
    reflectionPreparationStepPending = false;
    reflectionCaptureAssets = nullptr;
    ShutdownAtmosphereGpuQueries();
    atmosphereDiagnostics = SectorAtmosphereDiagnostics{};
    generatedGeometry = {};
    visibilityGraph = {};
    visibilityResult = {};
    portalVisibilityDebugText.clear();
    visibilityDebugText.clear();
    renderDebugText.clear();
    billboardRenderer.ResetDebugState();
    staticModelRenderer.ResetDebugState();
    visibilityLookupWorld = SectorCollisionWorld{};
    visibilityGraphValid = false;
    visibilityLookupWorldValid = false;
    dynamicLightState.Reset();
    ResetSectorStaticSpecularLights(staticSpecularLightState);
    surfaceLightmapBakeCurrent = false;
    objectProbeBakeCurrent = false;
    lightAtmosphereSources.clear();
    defaultMaterialTextureHandle = engine::NullTextureHandle();
    flashlightCookieTexture = engine::NullTextureHandle();
    doorRenderer.ClearPreparedShadowCasters();
    dynamicModelShadowRenderer.ClearPreparedShadowCasters();
    runtimeSeconds = 0.0f;
    distanceFogRenderer.Shutdown();
    analyticFogRenderer.Shutdown();
    analyticLightShaftRenderer.Shutdown();
    lightProxyRenderer.Shutdown();
    lightDustRenderer.Shutdown();
    underwaterRenderer.Shutdown();
    UnloadHdrSceneColorView();
    if (!initialized
            && engine::IsNull(assetScope)
            && meshes.batches.empty()
            && meshes.sectorDrawRecords.empty()
            && !materialLoaded
            && !bloomRenderer.IsLoaded()
            && hdrCompositeShader.id == 0
            && !engine::IsRenderTargetReady(hdrSceneScratch)
            && !billboardRenderer.IsLoaded()
            && !staticModelRenderer.IsLoaded()
            && !doorRenderer.HasOpaqueResources()
            && !doorRenderer.HasCachedDoorMeshes()
            && !windowRenderer.IsLoaded()
            && !liquidRenderer.IsLoaded()
            && !dynamicLightState.HasShadowMapResources()
            && !dynamicLightState.HasShadowMaterial()
            && !dynamicModelShadowRenderer.IsLoaded()
            && !skyRenderer.IsLoaded()) {
        return;
    }

    bloomRenderer.Shutdown();
    if (hdrCompositeShader.id != 0) UnloadShader(hdrCompositeShader);
    hdrCompositeShader = {};
    hdrCompositeSceneLoc = -1;
    hdrCompositeSourceLoc = -1;
    hdrCompositeModeLoc = -1;
    hdrCompositeShaderFailed = false;
    engine::UnloadRenderTarget(hdrSceneScratch);
    hdrSceneScratchError.clear();
    hdrSceneScratchDiagnostic = "not allocated";
    hdrSceneScratchFailedWidth = 0;
    hdrSceneScratchFailedHeight = 0;
    dynamicLightState.UnloadShadowMaterial();
    dynamicLightState.UnloadShadowMapResources();
    dynamicModelShadowRenderer.Shutdown();
    skyRenderer.Shutdown();
    pbrEnvironment = {};

    liquidRefractionFallbackLogged = false;
    atmosphereGpuFramePrepared = false;
    preGlassLightEffectsRendered = false;
    preGlassShaftApplied = false;
    preGlassHaloApplied = false;
    staticObjectAdjustmentBakedDataActive = false;
    doorRenderer.UnloadDoorMeshes();
    UnloadSectorMeshes(meshes);
    textureHandlesById.clear();
    normalTextureHandlesById.clear();
    propertyTextureHandlesById.clear();
    propertyMapKindsById.clear();
    normalStrengthById.clear();
    metallicFactorById.clear();
    roughnessFactorById.clear();
    lightmapTextures.clear();
    directionalLightmapTextures.clear();
    sectorCount = 0;

    if (materialLoaded) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = defaultMaterialTexture;
        material.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
        material.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
        material.maps[MATERIAL_MAP_HEIGHT].texture = Texture2D{};
        material.maps[MATERIAL_MAP_EMISSION].texture = Texture2D{};
        material.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
        material.maps[MATERIAL_MAP_BRDF].texture = Texture2D{};
        UnloadMaterial(material);
        material = Material{};
        defaultMaterialTexture = Texture2D{};
        materialLoaded = false;
        dynamicLightShadowSlotsLoc = -1;
        shadowLightMatrixLocs.fill(-1);
        shadowBiasLoc = -1;
        shadowStrengthLoc = -1;
        shadowSoftnessLoc = -1;
        dynamicLightProfilesLoc = -1;
        dynamicLightProfileParametersLoc = -1;
        flashlightCookieLoc = -1;
        staticSpecularLocations = {};
        materialPropertiesKindLoc = -1;
    }
    if (depthPrepassMaterialLoaded) {
        UnloadMaterial(depthPrepassMaterial);
        depthPrepassMaterial = Material{};
        depthPrepassMaterialLoaded = false;
    }

    billboardRenderer.Shutdown();
    staticModelRenderer.Shutdown();
    doorRenderer.ShutdownOpaqueResources();
    windowRenderer.Shutdown();
    ductCoverRenderer.Shutdown();
    liquidRenderer.Shutdown();

    if (!engine::IsNull(assetScope)) {
        assets.UnloadScope(assetScope);
        assetScope = engine::NullAssetScopeHandle();
    }

    initialized = false;
}

void SectorMeshRenderer::AdvanceRuntime(float dt)
{
    if (std::isfinite(dt) && dt > 0.0f) {
        runtimeSeconds += dt;
    }
}

void SectorMeshRenderer::FinalizeRuntimeObjectResources(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld)
{
    staticModelRenderer.FinalizeResources(
            assets,
            runtimeObjectWorld);
}

void SectorMeshRenderer::Render(
        engine::AssetManager& assets,
        bool useBakedAmbientOcclusion,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting,
        const SectorTopologyFogSettings& fogSettings)
{
    RenderDynamicSpotLightShadowMaps(assets, runtimeObjectWorld);
    UpdateRuntimeReflections(assets, runtimeObjectWorld, doorLighting);
    DrawScene(assets, useBakedAmbientOcclusion, runtimeObjectWorld, doorLighting, fogSettings);
}

void SectorMeshRenderer::DrawScene(
        engine::AssetManager& assets,
        bool useBakedAmbientOcclusion,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting,
        const SectorTopologyFogSettings& fogSettings,
        bool staticCaptureOnly,
        SectorUseHighlight useHighlight,
        SectorReflectionCaptureDrawContext* capture)
{
    if (!initialized) {
        return;
    }

    const Camera3D& camera = capture ? capture->camera : this->camera;
    const RuntimePortalVisibilityResult& visibilityResult = capture ? capture->visibility : this->visibilityResult;
    SectorDynamicLightingRenderer& dynamicLightState = capture ? *capture->lighting : this->dynamicLightState;
    const float runtimeSeconds = capture ? capture->seconds : this->runtimeSeconds;
    const bool dynamicLightingEnabled = capture ? true : this->dynamicLightingEnabled;
    const bool shadowMapsEnabled = capture ? true : this->shadowMapsEnabled;
    if (!capture) worldProfiler.BeginFrame(worldDiagnosticsEnabled);
    visibleSectorDraws.clear();
    if (!capture) worldProfiler.diagnostics.sectorTrianglesCulled = 0;
    const float viewAspect = capture ? 1.0f : static_cast<float>(rlGetFramebufferWidth())
            / std::max(1, rlGetFramebufferHeight());
    for (std::size_t index = 0; index < meshes.sectorDrawRecords.size(); ++index) {
        const auto& batch = meshes.sectorDrawRecords[index];
        if (!ShouldDrawSectorMeshRecordForVisibility(batch, visibilityResult)
                || (batch.hasBounds && !SectorBoundsInView(camera, viewAspect,
                        rlGetCullDistanceNear(), rlGetCullDistanceFar(), batch.bounds))) {
            if (capture) ++capture->batchesCulled;
            else worldProfiler.diagnostics.sectorTrianglesCulled += batch.triangleCount;
            continue;
        }
        AppendSectorOpaqueDraw(visibleSectorDraws, {engine::NullEntity(), batch.sectorId,
                index, MatrixIdentity(), batch.bounds, SectorNearestViewDepth(camera, batch.bounds)},
                sectorDrawCapacityWarned);
    }
    std::sort(visibleSectorDraws.begin(), visibleSectorDraws.end(), SectorOpaqueDrawLess);
    if (runtimeObjectWorld) {
        const auto& objectVisibility = capture ? capture->connectedVisibility : visibilityResult;
        staticModelRenderer.PrepareVisibleDraws(assets, *runtimeObjectWorld, camera, viewAspect, objectVisibility);
        doorRenderer.PrepareVisibleDraws(assets, *runtimeObjectWorld, camera, viewAspect, objectVisibility);
    }
    SectorPbrContributionSettings pbrContributionSettings = this->pbrContributionSettings;
    if (capture) {
        pbrContributionSettings.worldEnvironmentSpecularScale=0;
        pbrContributionSettings.worldIndirectDiffuseScale=1;
        pbrContributionSettings.reflectionCapture=true;
    }
    staticModelRenderer.SetPbrContributionSettings(pbrContributionSettings);
    dynamicLightState.SetFlashlightCookieTexture(
            assets.GetTexture(flashlightCookieTexture));

    BeginMode3D(camera);
    skyRenderer.Draw(assets, camera);
    if (!capture && depthPrepassEnabled && depthPrepassMaterialLoaded) {
        worldProfiler.Begin(SectorWorldStage::Depth);
        rlDrawRenderBatchActive();
        rlColorMask(false, false, false, false);
        rlEnableDepthTest();
        rlEnableDepthMask();
        DrawDepthPrepass(assets, runtimeObjectWorld);
        rlDrawRenderBatchActive();
        rlColorMask(true, true, true, true);
        worldProfiler.End();
    }

    if (!capture) worldProfiler.Begin(SectorWorldStage::Sectors);

    SectorTopologyFogSettings materialFogSettings = fogSettings;
    if (NormalizeSectorTopologyFogSettings(fogSettings).mode
            == SectorTopologyFogMode::Distance) {
        materialFogSettings.enabled = false;
    }
    const SectorFogRenderContext fogContext =
            BuildSectorFogRenderContext(materialFogSettings, camera.position);
    UploadSectorFogShaderValues(material.shader, fogShaderLocations, fogContext);

    float useAo = useBakedAmbientOcclusion ? 1.0f : 0.0f;
    const bool dynamicShadowsEnabled =
            shadowMapsEnabled && dynamicLightingEnabled;
    const Texture2D* shadowMap0 = dynamicShadowsEnabled
            ? dynamicLightState.ShadowMapDepthTexture(0) : nullptr;
    const Texture2D* shadowMap1 = dynamicShadowsEnabled
            ? dynamicLightState.ShadowMapDepthTexture(1) : nullptr;
    material.maps[MATERIAL_MAP_ROUGHNESS].texture = shadowMap0 != nullptr ? *shadowMap0 : Texture2D{};
    material.maps[MATERIAL_MAP_OCCLUSION].texture = shadowMap1 != nullptr ? *shadowMap1 : Texture2D{};
    constexpr float SectorSurfaceEnvironmentExposure = 0.15f;
    const int pbrDiagnosticMode = capture ? 11 : static_cast<int>(
            pbrContributionSettings.diagnosticMode);
    if (cameraPositionLoc >= 0) SetShaderValue(
            material.shader, cameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
    if (indirectDiffuseScaleLoc >= 0) SetShaderValue(
            material.shader, indirectDiffuseScaleLoc,
            &pbrContributionSettings.worldIndirectDiffuseScale,
            SHADER_UNIFORM_FLOAT);
    if (environmentSpecularScaleLoc >= 0) SetShaderValue(
            material.shader, environmentSpecularScaleLoc,
            &pbrContributionSettings.worldEnvironmentSpecularScale,
            SHADER_UNIFORM_FLOAT);
    if (pbrDiagnosticModeLoc >= 0) SetShaderValue(
            material.shader, pbrDiagnosticModeLoc,
            &pbrDiagnosticMode, SHADER_UNIFORM_INT);
    if (useBakedAmbientOcclusionLoc >= 0) {
        SetShaderValue(material.shader, useBakedAmbientOcclusionLoc, &useAo, SHADER_UNIFORM_FLOAT);
    }
    SectorDynamicLightShaderLocations dynamicLightLocations;
    dynamicLightLocations.dynamicLightCount = dynamicLightCountLoc;
    dynamicLightLocations.dynamicLightPositions = dynamicLightPositionsLoc;
    dynamicLightLocations.dynamicLightColors = dynamicLightColorsLoc;
    dynamicLightLocations.dynamicLightRadii = dynamicLightRadiiLoc;
    dynamicLightLocations.dynamicLightIntensities = dynamicLightIntensitiesLoc;
    dynamicLightLocations.dynamicLightTypes = dynamicLightTypesLoc;
    dynamicLightLocations.dynamicLightDirections = dynamicLightDirectionsLoc;
    dynamicLightLocations.dynamicLightInnerConeCos = dynamicLightInnerConeCosLoc;
    dynamicLightLocations.dynamicLightOuterConeCos = dynamicLightOuterConeCosLoc;
    dynamicLightLocations.dynamicLightSpotShadowRight = dynamicLightSpotShadowRightLoc;
    dynamicLightLocations.dynamicLightSpotShadowProjection =
            dynamicLightSpotShadowProjectionLoc;
    dynamicLightLocations.dynamicLightProfiles = dynamicLightProfilesLoc;
    dynamicLightLocations.dynamicLightProfileParameters =
            dynamicLightProfileParametersLoc;
    dynamicLightLocations.flashlightCookie = flashlightCookieLoc;
    dynamicLightLocations.hasPointShadows = hasPointShadowsLoc;
    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    shadowLocations.shadowAtlasTilesPerRow = shadowAtlasTilesPerRowLoc;
    dynamicLightState.BuildSectorLightContexts(
            meshes.sectorReceiverBounds,
            dynamicLightingEnabled,
            dynamicShadowsEnabled,
            runtimeSeconds);
    const SectorBillboardDynamicLightContext fallbackLightContext =
            dynamicLightState.BuildLightContext(
                    nullptr,
                    dynamicLightingEnabled,
                    dynamicShadowsEnabled,
                    runtimeSeconds);
    UploadSectorRendererDynamicSpotLightShadowUniforms(
            material.shader,
            shadowLocations,
            fallbackLightContext.shadowUniforms);
    int uploadedLightSectorId = std::numeric_limits<int>::min();
    const Texture2D* loadedDefaultMaterialTexture =
            assets.GetTexture(defaultMaterialTextureHandle);
    const Texture2D& activeDefaultMaterialTexture =
            loadedDefaultMaterialTexture != nullptr
            ? *loadedDefaultMaterialTexture
            : defaultMaterialTexture;
    for (const auto& item : visibleSectorDraws) {
        const SectorMeshBatch& batch = meshes.sectorDrawRecords[item.index];
        if (capture) ++capture->batchesDrawn;

        if (batch.sectorId != uploadedLightSectorId) {
            const SectorBillboardDynamicLightContext* lightContext =
                    dynamicLightState.FindSectorLightContext(batch.sectorId);
            if (lightContext == nullptr) lightContext = &fallbackLightContext;
            UploadSectorRendererDynamicPointLights(
                    material.shader, dynamicLightLocations, *lightContext);
            UploadSectorRendererDynamicShadowSlots(
                    material.shader,
                    shadowLocations.dynamicLightShadowSlots,
                    lightContext->shadowUniforms);
            const SectorReceiverBounds* receiverBounds =
                    FindSectorReceiverBounds(
                            meshes.sectorReceiverBounds, batch.sectorId);
            const SectorReceiverBounds fallbackBounds{
                    batch.sectorId, camera.position, camera.position};
            const SectorReceiverBounds& environmentBounds = receiverBounds != nullptr
                    ? *receiverBounds : fallbackBounds;
            const Vector3 environmentReceiver = Vector3Scale(
                    Vector3Add(environmentBounds.min, environmentBounds.max),
                    0.5f);
            const SectorPbrEnvironmentSelection environmentSelection =
                    SelectSectorPbrEnvironment(
                            pbrEnvironment,
                            environmentReceiver,
                            batch.sectorId,
                            true);
            const TextureCubemap* selectedEnvironment = assets.GetCubemap(
                    environmentSelection.cubemap);
            const int hasEnvironment = selectedEnvironment != nullptr
                            && selectedEnvironment->id != 0
                            && pbrContributionSettings.worldEnvironmentSpecularScale > 0.0f
                    ? 1 : 0;
            material.maps[MATERIAL_MAP_CUBEMAP].texture = hasEnvironment != 0
                    ? *selectedEnvironment : Texture2D{};
            const float environmentExposure = environmentSelection.localProbe
                    ? 1.0f : SectorSurfaceEnvironmentExposure;
            const int boxProjection = environmentSelection.boxProjection ? 1 : 0;
            if (hasEnvironmentLoc >= 0) SetShaderValue(material.shader,
                    hasEnvironmentLoc, &hasEnvironment, SHADER_UNIFORM_INT);
            if (environmentExposureLoc >= 0) SetShaderValue(material.shader,
                    environmentExposureLoc, &environmentExposure, SHADER_UNIFORM_FLOAT);
            if (environmentBoxProjectionLoc >= 0) SetShaderValue(material.shader,
                    environmentBoxProjectionLoc, &boxProjection, SHADER_UNIFORM_INT);
            if (environmentCapturePositionLoc >= 0) SetShaderValue(material.shader,
                    environmentCapturePositionLoc,
                    &environmentSelection.capturePosition, SHADER_UNIFORM_VEC3);
            if (environmentInfluenceCenterLoc >= 0) SetShaderValue(material.shader,
                    environmentInfluenceCenterLoc,
                    &environmentSelection.influenceCenter, SHADER_UNIFORM_VEC3);
            if (environmentHalfExtentsLoc >= 0) SetShaderValue(material.shader,
                    environmentHalfExtentsLoc,
                    &environmentSelection.halfExtents, SHADER_UNIFORM_VEC3);
            if (environmentYawLoc >= 0) SetShaderValue(material.shader,
                    environmentYawLoc, &environmentSelection.yawRadians, SHADER_UNIFORM_FLOAT);
            if (environmentMaxLodLoc >= 0) SetShaderValue(material.shader,
                    environmentMaxLodLoc, &environmentSelection.maxLod, SHADER_UNIFORM_FLOAT);
            if (environmentIntensityLoc >= 0) SetShaderValue(material.shader,
                    environmentIntensityLoc, &environmentSelection.intensity, SHADER_UNIFORM_FLOAT);
            const SectorStaticSpecularLightContext staticSpecularContext =
                    SelectSectorStaticSpecularLights(
                            staticSpecularLightState,
                            receiverBounds != nullptr
                                    ? *receiverBounds
                                    : fallbackBounds,
                            batch.sectorId,
                            visibilityResult,
                            surfaceLightmapBakeCurrent && !staticCaptureOnly);
            UploadSectorStaticSpecularLights(
                    material.shader,
                    staticSpecularLocations,
                    staticSpecularContext);
            uploadedLightSectorId = batch.sectorId;
        }

        const engine::TextureHandle textureHandle = TextureForId(batch.materialId);
        const Texture2D* texture = assets.GetTexture(textureHandle);
        material.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                ? *texture
                : activeDefaultMaterialTexture;

        const auto* reflectionBounds = FindSectorReceiverBounds(meshes.sectorReceiverBounds, batch.sectorId);
        const BoundingBox reflectionBox = reflectionBounds
                ? BoundingBox{reflectionBounds->min, reflectionBounds->max}
                : BoundingBox{camera.position, camera.position};
        const auto reflectionBlend = capture ? SectorPbrEnvironmentBlend{} :
                SelectSectorPbrEnvironmentBlend(pbrEnvironment,
                        Vector3Scale(Vector3Add(reflectionBox.min, reflectionBox.max), 0.5f),
                        batch.sectorId, true, &reflectionBox,
                        !batch.hasBounds || SectorReflectionBoundsInView(camera,
                                static_cast<float>(rlGetFramebufferWidth()) / std::max(1, rlGetFramebufferHeight()),
                                rlGetCullDistanceNear(), rlGetCullDistanceFar(), batch.bounds)
                                ? pbrEnvironment.demandCollector : nullptr);
        UploadSectorReflectionBlend(material.shader, reflectionLocations, reflectionBlend, assets);
        const Texture2D* normalTexture = assets.GetTexture(
                NormalTextureForId(batch.materialId));
        const Texture2D* propertyTexture = assets.GetTexture(
                PropertyTextureForId(batch.materialId));
        const int propertyMapKind = propertyTexture != nullptr
                ? static_cast<int>(PropertyMapKindForId(batch.materialId))
                : static_cast<int>(SectorMaterialPropertyMapKind::None);

        const Texture2D* decalTexture = nullptr;
        if (!batch.decalMaterialId.empty()) {
            decalTexture = assets.GetTexture(TextureForId(batch.decalMaterialId));
        }

        const Texture2D* lightmap = batch.lightmapAtlasIndex >= 0
                && batch.lightmapAtlasIndex
                        < static_cast<int>(lightmapTextures.size())
                ? assets.GetTexture(lightmapTextures[
                        static_cast<size_t>(batch.lightmapAtlasIndex)])
                : nullptr;
        const Texture2D* directionalLightmap = batch.lightmapAtlasIndex >= 0
                && batch.lightmapAtlasIndex
                        < static_cast<int>(directionalLightmapTextures.size())
                ? assets.GetTexture(directionalLightmapTextures[
                        static_cast<size_t>(batch.lightmapAtlasIndex)])
                : nullptr;
        const float useLightmap = lightmap != nullptr ? 1.0f : 0.0f;
        material.maps[MATERIAL_MAP_SPECULAR].texture = lightmap != nullptr
                ? *lightmap
                : Texture2D{};
        const int hasDecal = decalTexture != nullptr ? 1 : 0;
        const int hasLightmap = batch.receivesLightmap
                && lightmap != nullptr ? 1 : 0;
        const int hasDirectionalLightmap = hasLightmap != 0
                && directionalLightmap != nullptr ? 1 : 0;
        const int hasNormalMap = normalTexture != nullptr ? 1 : 0;
        const auto normalStrengthIt = normalStrengthById.find(batch.materialId);
        const float materialNormalStrength = normalStrengthIt == normalStrengthById.end()
                ? 1.0f
                : normalStrengthIt->second;
        const auto metallicIt = metallicFactorById.find(batch.materialId);
        const float materialMetallic = metallicIt == metallicFactorById.end()
                ? 0.0f
                : metallicIt->second;
        const auto roughnessIt = roughnessFactorById.find(batch.materialId);
        const float materialRoughness = roughnessIt == roughnessFactorById.end()
                ? 0.8f
                : roughnessIt->second;
        const int useStaticSpecularLighting = surfaceLightmapBakeCurrent
                && hasLightmap != 0 ? 1 : 0;
        const int alphaTest = batch.alphaTest ? 1 : 0;
        const float alphaCutoff = batch.alphaCutoff;
        const float decalOpacity = batch.decalOpacity;
        const int decalEmissive = hasDecal != 0 && batch.decalEmissive ? 1 : 0;
        const Vector3 decalTint = hasDecal != 0
                ? engine::SrgbNormalizedRgbToLinearScene(batch.decalTint)
                : Vector3{1.0f, 1.0f, 1.0f};
        material.maps[MATERIAL_MAP_NORMAL].texture = (decalTexture != nullptr)
                ? *decalTexture
                : Texture2D{};
        material.maps[MATERIAL_MAP_HEIGHT].texture = (normalTexture != nullptr)
                ? *normalTexture
                : Texture2D{};
        material.maps[MATERIAL_MAP_EMISSION].texture =
                directionalLightmap != nullptr
                ? *directionalLightmap
                : Texture2D{};
        material.maps[MATERIAL_MAP_BRDF].texture = propertyTexture != nullptr
                ? *propertyTexture
                : Texture2D{};
        if (useLightmapLoc >= 0) {
            SetShaderValue(
                    material.shader,
                    useLightmapLoc,
                    &useLightmap,
                    SHADER_UNIFORM_FLOAT);
        }
        if (hasLightmapLoc >= 0) {
            SetShaderValue(material.shader, hasLightmapLoc, &hasLightmap, SHADER_UNIFORM_INT);
        }
        if (hasDirectionalLightmapLoc >= 0) {
            SetShaderValue(
                    material.shader,
                    hasDirectionalLightmapLoc,
                    &hasDirectionalLightmap,
                    SHADER_UNIFORM_INT);
        }
        if (hasNormalMapLoc >= 0) {
            SetShaderValue(material.shader, hasNormalMapLoc, &hasNormalMap, SHADER_UNIFORM_INT);
        }
        if (normalStrengthLoc >= 0) {
            SetShaderValue(
                    material.shader,
                    normalStrengthLoc,
                    &materialNormalStrength,
                    SHADER_UNIFORM_FLOAT);
        }
        if (materialPropertiesKindLoc >= 0) SetShaderValue(
                material.shader, materialPropertiesKindLoc,
                &propertyMapKind, SHADER_UNIFORM_INT);
        if (metallicFactorLoc >= 0) SetShaderValue(
                material.shader, metallicFactorLoc,
                &materialMetallic, SHADER_UNIFORM_FLOAT);
        if (roughnessFactorLoc >= 0) SetShaderValue(
                material.shader, roughnessFactorLoc,
                &materialRoughness, SHADER_UNIFORM_FLOAT);
        if (useStaticSpecularLightingLoc >= 0) SetShaderValue(
                material.shader, useStaticSpecularLightingLoc,
                &useStaticSpecularLighting, SHADER_UNIFORM_INT);
        if (alphaTestLoc >= 0) {
            SetShaderValue(material.shader, alphaTestLoc, &alphaTest, SHADER_UNIFORM_INT);
        }
        if (alphaCutoffLoc >= 0) {
            SetShaderValue(material.shader, alphaCutoffLoc, &alphaCutoff, SHADER_UNIFORM_FLOAT);
        }
        if (hasDecalLoc >= 0) {
            SetShaderValue(material.shader, hasDecalLoc, &hasDecal, SHADER_UNIFORM_INT);
        }
        if (decalOpacityLoc >= 0) {
            SetShaderValue(material.shader, decalOpacityLoc, &decalOpacity, SHADER_UNIFORM_FLOAT);
        }
        if (decalEmissiveLoc >= 0) {
            SetShaderValue(material.shader, decalEmissiveLoc, &decalEmissive, SHADER_UNIFORM_INT);
        }
        if (decalEmissiveStrengthLoc >= 0) {
            const float emissiveStrength = batch.decalEmissiveStrength;
            SetShaderValue(material.shader, decalEmissiveStrengthLoc, &emissiveStrength, SHADER_UNIFORM_FLOAT);
        }
        if (decalTintLoc >= 0) {
            SetShaderValue(material.shader, decalTintLoc, &decalTint, SHADER_UNIFORM_VEC3);
        }
        DrawMesh(batch.mesh, material, MatrixIdentity());
    }
    if (!capture) {
        worldProfiler.End();
        auto& stats = worldProfiler.diagnostics;
        stats.sectorMeshes = visibleSectorDraws.size();
        stats.sectorCulled = meshes.sectorDrawRecords.size() - visibleSectorDraws.size();
        stats.sectorTriangles = 0;
        for (const auto& item : visibleSectorDraws) stats.sectorTriangles += meshes.sectorDrawRecords[item.index].triangleCount;
        stats.objects = stats.objectsCulled = stats.modelMeshes = stats.modelTriangles = 0;
        stats.modelMeshesCulled = stats.modelTrianglesCulled = 0;
        worldProfiler.Begin(SectorWorldStage::ModelsDoors);
    }
    if (runtimeObjectWorld != nullptr) {
        const SectorPbrEnvironmentSelection objectEnvironmentSelection =
                SelectSectorPbrEnvironment(
                        pbrEnvironment,
                        camera.position,
                        -1,
                        true);
        const TextureCubemap* environmentTexture = assets.GetCubemap(
                objectEnvironmentSelection.cubemap);
        const bool environmentReady = environmentTexture != nullptr
                && environmentTexture->id != 0;
        SectorDoorDrawContext doorDrawContext;
        doorDrawContext.captureCulling = capture ? &capture->culling : nullptr;
        doorDrawContext.reflectionEnvironment = capture ? nullptr : &pbrEnvironment;
        doorDrawContext.assets = &assets;
        doorDrawContext.runtimeObjectWorld = runtimeObjectWorld;
        doorDrawContext.lighting = doorLighting;
        doorDrawContext.dynamicLighting.enabled = dynamicLightingEnabled;
        doorDrawContext.dynamicLighting.runtimeSeconds = runtimeSeconds;
        doorDrawContext.dynamicLighting.selectedLights = &dynamicLightState.SelectedLights();
        doorDrawContext.dynamicLighting.shadowUniforms =
                dynamicLightState.PackShadowUniforms(shadowMapsEnabled);
        doorDrawContext.dynamicLighting.shadowMaps = dynamicLightState.BuildShadowMapTextures();
        doorDrawContext.fog = fogContext;
        doorDrawContext.materialResolver.userData = this;
        doorDrawContext.materialResolver.resolve = &SectorMeshRenderer::ResolveDoorMaterial;
        doorDrawContext.camera = camera;
        doorDrawContext.pbr = pbrContributionSettings;
        doorDrawContext.staticSpecularLights = &staticSpecularLightState;
        // Object bounds may straddle their owning sector or a portal aperture.
        // Keep connected membership conservative, then cull the actual bounds.
        doorDrawContext.visibility = capture ? &capture->connectedVisibility : &visibilityResult;
        doorDrawContext.environment = environmentReady
                ? environmentTexture
                : nullptr;
        doorDrawContext.environmentExposure = objectEnvironmentSelection.localProbe
                ? objectEnvironmentSelection.intensity
                : SectorSurfaceEnvironmentExposure;
        doorDrawContext.environmentCapturePosition = objectEnvironmentSelection.capturePosition;
        doorDrawContext.environmentInfluenceCenter = objectEnvironmentSelection.influenceCenter;
        doorDrawContext.environmentHalfExtents = objectEnvironmentSelection.halfExtents;
        doorDrawContext.environmentYaw = objectEnvironmentSelection.yawRadians;
        doorDrawContext.environmentMaxLod = objectEnvironmentSelection.maxLod;
        doorDrawContext.environmentBoxProjection = objectEnvironmentSelection.boxProjection;
        doorDrawContext.staticSpecularEligible = !staticCaptureOnly
                && objectProbeBakeCurrent
                && doorLighting.objectLightProbes != nullptr
                && !doorLighting.objectLightProbes->probes.empty();
        doorDrawContext.defaultMaterialTexture = &activeDefaultMaterialTexture;
        doorDrawContext.renderDebugText = &renderDebugText;
        doorRenderer.Draw(doorDrawContext);
        ductCoverRenderer.Draw(doorDrawContext, doorRenderer);

        const SectorBillboardDynamicLightContext billboardLightContext = dynamicLightState.BuildLightContext(
                nullptr,dynamicLightingEnabled,shadowMapsEnabled,runtimeSeconds);
        const TextureCubemap* pbrEnvironmentTexture = environmentReady
                ? environmentTexture : nullptr;
        staticModelRenderer.SetReflectionEnvironment(capture ? nullptr : &pbrEnvironment);
        staticModelRenderer.SetEnvironmentProjection(objectEnvironmentSelection);
        staticModelRenderer.Draw(
                assets,
                *runtimeObjectWorld,
                camera,
                billboardLightContext,
                staticSpecularLightState,
                surfaceLightmapBakeCurrent,
                objectProbeBakeCurrent
                        && doorLighting.objectLightProbes != nullptr
                        && !doorLighting.objectLightProbes->probes.empty(),
                fogContext,
                capture ? capture->connectedVisibility : visibilityResult,
                lightmapTextures,
                pbrEnvironmentTexture,
                useBakedAmbientOcclusion,
                renderDebugText,
                staticCaptureOnly,
                useHighlight,
                capture ? &capture->culling : nullptr);
        if (!staticCaptureOnly) {
            SectorDynamicModelShadowDrawContext modelShadowContext;
            modelShadowContext.assets = &assets;
            modelShadowContext.world = runtimeObjectWorld;
            modelShadowContext.collisionWorld = visibilityLookupWorldValid
                    ? &visibilityLookupWorld
                    : nullptr;
            modelShadowContext.visibility = &visibilityResult;
            dynamicModelShadowRenderer.Draw(modelShadowContext);
            billboardRenderer.Draw(
                    assets,
                    *runtimeObjectWorld,
                    camera,
                    billboardLightContext,
                    fogContext,
                    renderDebugText);
        }
    }
    if (!capture) {
        worldProfiler.End();
        if (runtimeObjectWorld) {
            auto& stats = worldProfiler.diagnostics;
            stats.objects = staticModelRenderer.VisibleOpaqueObjects() + doorRenderer.VisibleOpaqueObjects();
            stats.objectsCulled = staticModelRenderer.CulledOpaqueObjects() + doorRenderer.CulledOpaqueObjects();
            stats.modelMeshes = staticModelRenderer.SubmittedMeshes() + doorRenderer.VisibleOpaqueObjects();
            stats.modelTriangles = staticModelRenderer.SubmittedTriangles() + doorRenderer.SubmittedTriangles();
            stats.modelMeshesCulled = staticModelRenderer.CulledMeshes() + doorRenderer.CulledOpaqueObjects();
            stats.modelTrianglesCulled = staticModelRenderer.CulledTriangles() + doorRenderer.CulledTriangles();
        }
    }
    EndMode3D();
}

void SectorMeshRenderer::UpdateRuntimeReflections(engine::AssetManager& assets,
        engine::World* world, SectorRuntimeDoorLightingContext lighting, bool preparing)
{
    if (!initialized) return;
    if (!preparing) runtimeReflections.BeginMainViewFrame();
    if (pbrEnvironment.demandCollector) {
        auto& demand = *pbrEnvironment.demandCollector;
        demand.camera = camera;
        demand.aspect = static_cast<float>(rlGetFramebufferWidth()) / std::max(1, rlGetFramebufferHeight());
        demand.nearPlane = rlGetCullDistanceNear();
        demand.farPlane = rlGetCullDistanceFar();
    }
    // Preview may reveal its main view in the same frame as the final loading
    // tile. Do not spend a second capture budget at that handoff.
    if (!preparing && reflectionPreparationStepPending) {
        reflectionPreparationStepPending=false;
        return;
    }
    reflectionPreparationStepPending=preparing;
    pbrEnvironment.seconds = GetTime();
    runtimeReflections.ObserveLights(pbrEnvironment, dynamicLightState, runtimeSeconds);
    if (preparing) runtimeReflections.RequireInitial(pbrEnvironment, visibilityResult, visibilityResult.startSectorId);
    runtimeReflections.Step(assets, pbrEnvironment, *this, world, lighting, preparing);
}

void SectorMeshRenderer::PrepareReflectionCapture(SectorReflectionCaptureDrawContext& draw,
        const SectorCompiledReflectionProbe& probe, engine::World* world)
{
    // Reuse reserved storage for a flood of sectors reachable from the probe.
    auto& v = draw.connectedVisibility;
    v.visibleSectorIds.clear();
    v.boundarySurfaceSectorIds.clear();
    v.visibleSectorIds.push_back(probe.topologySectorId);
    for (std::size_t i=0; i<v.visibleSectorIds.size(); ++i) {
        for (const auto& edge : pbrEnvironment.portals) {
            if (!edge.open || edge.fromSectorId!=v.visibleSectorIds[i]) continue;
            const bool blocked=std::any_of(pbrEnvironment.blockers.begin(),pbrEnvironment.blockers.end(),
                    [&](const auto& b){ return b.lineDefId==edge.lineDefId && b.blocksPortal; });
            if (blocked) {
                v.boundarySurfaceSectorIds.push_back(edge.toSectorId);
                continue;
            }
            if (std::find(v.visibleSectorIds.begin(),v.visibleSectorIds.end(),edge.toSectorId)!=v.visibleSectorIds.end()) continue;
            v.visibleSectorIds.push_back(edge.toSectorId);
        }
    }
    std::sort(v.visibleSectorIds.begin(),v.visibleSectorIds.end());
    auto& boundary = v.boundarySurfaceSectorIds;
    std::sort(boundary.begin(), boundary.end());
    boundary.erase(std::unique(boundary.begin(), boundary.end()), boundary.end());
    boundary.erase(std::remove_if(boundary.begin(), boundary.end(), [&](int sector) {
        return std::binary_search(v.visibleSectorIds.begin(), v.visibleSectorIds.end(), sector);
    }), boundary.end());
    v.startSectorId=probe.topologySectorId;v.validStartSector=true;v.fallbackDrawAll=false;
    v.startSectorIds.assign(1, probe.topologySectorId);
    draw.lighting->UpdateSelection(v,probe.topologySectorId,meshes.sectorReceiverBounds,world);
}

void SectorMeshRenderer::PrepareReflectionShadows(SectorReflectionCaptureDrawContext& draw,
        engine::World* world)
{
    draw.lighting->BeginShadowFrame(true);
    SectorDynamicSpotLightShadowRenderContext context;
    context.assets=reflectionCaptureAssets;
    context.sectorDrawRecords=&meshes.sectorDrawRecords;
    context.sectorReceiverBounds=&meshes.sectorReceiverBounds;
    context.userData=this;context.textureResolver=&SectorMeshRenderer::ResolveShadowCasterTexture;
    doorRenderer.PrepareShadowRenderContext(context,world);
    staticModelRenderer.PrepareShadowRenderContext(context,world);
    draw.lighting->RenderShadowMaps(context);
}

void SectorMeshRenderer::DrawReflectionFace(engine::AssetManager& assets,
        SectorReflectionCaptureDrawContext& draw,const SectorCompiledReflectionProbe& probe,
        int face,engine::RenderTarget& target,engine::World* world,SectorRuntimeDoorLightingContext lighting)
{
    draw.camera=SectorReflectionFaceCamera(probe.capturePositionWorld,face);
    draw.culling.camera = draw.camera;
    draw.culling.nearPlane = rlGetCullDistanceNear();
    draw.culling.farPlane = rlGetCullDistanceFar();
    draw.visibility = ComputeRuntimeSectorCaptureVisibility(
            visibilityGraph, draw.camera, draw.connectedVisibility, &pbrEnvironment.blockers,
            0, &visibilityScratch);
    BeginTextureMode(target.native);ClearBackground(BLACK);
    DrawScene(assets,true,world,lighting,SectorTopologyFogSettings{},true,{},&draw);
    rlDrawRenderBatchActive();EndTextureMode();
}

void SectorMeshRenderer::DrawDepthPrepass(
        engine::AssetManager& assets,
        engine::World* runtimeObjectWorld)
{
    RestoreSectorMaterialCulling();
    for (const auto& item : visibleSectorDraws) {
        const auto& batch = meshes.sectorDrawRecords[item.index];
        if (!batch.alphaTest) DrawMesh(batch.mesh, depthPrepassMaterial, MatrixIdentity());
    }
    if (!runtimeObjectWorld) return;
    doorRenderer.DrawPreparedDepth(depthPrepassMaterial);
    staticModelRenderer.DrawPreparedDepth(assets, *runtimeObjectWorld,
            depthPrepassMaterial, lightmapTextures);
}

SectorBillboardDynamicLightContext SectorMeshRenderer::BuildBillboardDynamicLightContext() const
{
    return dynamicLightState.BuildLightContext(
            nullptr,
            dynamicLightingEnabled,
            shadowMapsEnabled && dynamicLightingEnabled,
            runtimeSeconds);
}

void SectorMeshRenderer::SetPlayerFlashlight(
        const SectorPreviewDynamicPointLightSource* light)
{
    dynamicLightState.SetReservedRuntimeLight(light);
}

void SectorMeshRenderer::DrawViewmodel(
        engine::AssetManager& assets,
        const engine::ModelAsset& asset,
        engine::AnimatedModelInstance& instance,
        const Camera3D& viewmodelCamera,
        Matrix transform,
        const engine::ModelAsset* attachmentAsset,
        Matrix attachmentTransform,
        int receiverSectorId,
        bool objectProbeRuntimeAvailable,
        const BakedObjectLightingVerticalSample& ambientLighting,
        const SectorViewmodelLightingContext& lighting,
        const SectorViewmodelLightingContext& attachmentLighting)
{
    BeginMode3D(viewmodelCamera);
    const SectorPbrEnvironmentBlend viewmodelBlend =
            SelectSectorPbrEnvironmentBlend(
                    pbrEnvironment,
                    viewmodelCamera.position,
                    receiverSectorId,
                    true, nullptr, pbrEnvironment.demandCollector);
    const auto& viewmodelEnvironment = viewmodelBlend.first;
    const TextureCubemap* pbrEnvironmentTexture = assets.GetCubemap(
            viewmodelEnvironment.cubemap);
    if (pbrEnvironmentTexture != nullptr && pbrEnvironmentTexture->id == 0) {
        pbrEnvironmentTexture = nullptr;
    }
    const SectorReceiverBounds receiverBounds{
            receiverSectorId,
            viewmodelCamera.position,
            viewmodelCamera.position};
    const bool validProbe = ambientLighting.lower.valid
            || ambientLighting.upper.valid;
    const bool currentProbeForDraw = objectProbeBakeCurrent
            && objectProbeRuntimeAvailable;
    const SectorStaticSpecularLightContext staticSpecularContext =
            SelectSectorStaticSpecularLights(
                    staticSpecularLightState,
                    receiverBounds,
                    receiverSectorId,
                    visibilityResult,
                    currentProbeForDraw && validProbe);
    staticModelRenderer.SetEnvironmentBlend(viewmodelBlend,viewmodelCamera.position);
    staticModelRenderer.DrawViewmodel(
            assets, asset, instance, viewmodelCamera, transform,
            attachmentAsset, attachmentTransform,
            BuildBillboardDynamicLightContext(),
            staticSpecularContext,
            currentProbeForDraw,
            pbrEnvironmentTexture,
            ambientLighting,
            lighting,
            attachmentLighting);
    EndMode3D();
}

void SectorMeshRenderer::RenderDynamicSpotLightShadowMaps(
        engine::AssetManager& assets,
        engine::World* runtimeObjectWorld)
{
    dynamicLightState.BeginShadowFrame(
            shadowMapsEnabled && dynamicLightingEnabled);
    if (!shadowMapsEnabled) {
        return;
    }
    if (dynamicLightingEnabled && dynamicLightState.IsShadowRenderReady()) {
        SectorDynamicSpotLightShadowRenderContext context;
        context.assets = &assets;
        context.sectorDrawRecords = &meshes.sectorDrawRecords;
        context.sectorReceiverBounds = &meshes.sectorReceiverBounds;
        context.userData = this;
        context.textureResolver = &SectorMeshRenderer::ResolveShadowCasterTexture;
        doorRenderer.PrepareShadowRenderContext(context, runtimeObjectWorld);
        staticModelRenderer.PrepareShadowRenderContext(
                context,
                runtimeObjectWorld);
        dynamicModelShadowRenderer.PrepareShadowRenderContext(
                context,
                runtimeObjectWorld);
        dynamicLightState.RenderShadowMaps(context);
    }
}

bool SectorMeshRenderer::EnsureHdrSceneScratch(
        const engine::RenderTarget& sceneTarget)
{
    const int width = sceneTarget.native.texture.width;
    const int height = sceneTarget.native.texture.height;
    if (engine::IsRenderTargetReady(hdrSceneScratch)
            && hdrSceneScratch.native.texture.width == width
            && hdrSceneScratch.native.texture.height == height) {
        return true;
    }
    if (hdrSceneScratchFailedWidth == width
            && hdrSceneScratchFailedHeight == height
            && !hdrSceneScratchError.empty()) {
        return false;
    }
    engine::UnloadRenderTarget(hdrSceneScratch);
    std::string error;
    if (!engine::LoadRenderTarget(
                engine::RenderTargetDescriptor{
                        "hdr-effect-scene-scratch",
                        width,
                        height,
                        engine::RenderTargetColorFormat::Rgba32Float,
                        engine::RenderTargetFilter::Point,
                        engine::RenderTargetWrap::Clamp,
                        engine::RenderTargetDepthKind::None,
                        1},
                hdrSceneScratch,
                &error)) {
        hdrSceneScratchError = error;
        hdrSceneScratchDiagnostic = "disabled: " + error;
        hdrSceneScratchFailedWidth = width;
        hdrSceneScratchFailedHeight = height;
        TraceLog(LOG_WARNING, "HDR EFFECTS: shared RGBA32F scratch unavailable: %s",
                error.c_str());
        return false;
    }
    hdrSceneScratchError.clear();
    hdrSceneScratchDiagnostic = engine::FormatRenderTargetDiagnostic(hdrSceneScratch);
    hdrSceneScratchFailedWidth = 0;
    hdrSceneScratchFailedHeight = 0;
    return true;
}

bool SectorMeshRenderer::EnsureHdrSceneColorView(
        const engine::RenderTarget& sceneTarget)
{
    if (hdrSceneColorView.id != 0
            && hdrSceneColorView.texture.id == sceneTarget.native.texture.id) {
        return true;
    }
    UnloadHdrSceneColorView();
    hdrSceneColorView.id = rlLoadFramebuffer();
    hdrSceneColorView.texture = sceneTarget.native.texture;
    if (hdrSceneColorView.id == 0) {
        hdrSceneColorView = {};
        return false;
    }
    rlEnableFramebuffer(hdrSceneColorView.id);
    rlFramebufferAttach(
            hdrSceneColorView.id,
            sceneTarget.native.texture.id,
            RL_ATTACHMENT_COLOR_CHANNEL0,
            RL_ATTACHMENT_TEXTURE2D,
            0);
    const bool complete = rlFramebufferComplete(hdrSceneColorView.id);
    rlDisableFramebuffer();
    if (!complete) {
        UnloadHdrSceneColorView();
    }
    return complete;
}

void SectorMeshRenderer::UnloadHdrSceneColorView()
{
    if (hdrSceneColorView.id != 0) {
        rlUnloadFramebuffer(hdrSceneColorView.id);
    }
    hdrSceneColorView = {};
}

bool SectorMeshRenderer::InitializeHdrCompositeShader()
{
    if (hdrCompositeShader.id != 0) return true;
    if (hdrCompositeShaderFailed) return false;
    hdrCompositeShader = LoadGameShader(GameShader::HdrComposite);
    if (hdrCompositeShader.id == 0) { hdrCompositeShaderFailed=true; return false; }
    hdrCompositeSceneLoc = GetShaderLocation(hdrCompositeShader, "sceneColor");
    hdrCompositeSourceLoc = GetShaderLocation(hdrCompositeShader, "sourceColor");
    hdrCompositeModeLoc = GetShaderLocation(hdrCompositeShader, "compositeMode");
    if (hdrCompositeSceneLoc < 0 || hdrCompositeSourceLoc < 0 || hdrCompositeModeLoc < 0) {
        UnloadShader(hdrCompositeShader);
        hdrCompositeShader = {};
        hdrCompositeShaderFailed = true;
        return false;
    }
    return true;
}

bool SectorMeshRenderer::CommitHdrScratch(engine::RenderTarget& sceneTarget)
{
    if (hdrCompositeShader.id == 0) return false;
    const int mode = 0;
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneTarget.native);
    BeginShaderMode(hdrCompositeShader);
    SetShaderValueTexture(hdrCompositeShader, hdrCompositeSourceLoc, hdrSceneScratch.native.texture);
    SetShaderValue(hdrCompositeShader, hdrCompositeModeLoc, &mode, SHADER_UNIFORM_INT);
    rlDisableColorBlend();
    DrawTexturePro(
            hdrSceneScratch.native.texture,
            Rectangle{0, 0, static_cast<float>(hdrSceneScratch.native.texture.width),
                    -static_cast<float>(hdrSceneScratch.native.texture.height)},
            Rectangle{0, 0, static_cast<float>(sceneTarget.native.texture.width),
                    static_cast<float>(sceneTarget.native.texture.height)},
            Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndShaderMode();
    EndTextureMode();
    return true;
}

bool SectorMeshRenderer::CompositeViewmodel(
        engine::RenderTarget& sceneTarget,
        const engine::RenderTarget& viewmodelTarget)
{
    if (!initialized || !EnsureHdrSceneScratch(sceneTarget) || hdrCompositeShader.id == 0
            || viewmodelTarget.descriptor.colorFormat
                    != engine::RenderTargetColorFormat::Rgba32Float) {
        return false;
    }
    const int mode = 1;
    rlDrawRenderBatchActive();
    BeginTextureMode(hdrSceneScratch.native);
    ClearBackground(BLANK);
    BeginShaderMode(hdrCompositeShader);
    SetShaderValueTexture(hdrCompositeShader, hdrCompositeSceneLoc, sceneTarget.native.texture);
    SetShaderValueTexture(hdrCompositeShader, hdrCompositeSourceLoc, viewmodelTarget.native.texture);
    SetShaderValue(hdrCompositeShader, hdrCompositeModeLoc, &mode, SHADER_UNIFORM_INT);
    rlDisableColorBlend();
    DrawTexturePro(
            sceneTarget.native.texture,
            Rectangle{0, 0, static_cast<float>(sceneTarget.native.texture.width),
                    -static_cast<float>(sceneTarget.native.texture.height)},
            Rectangle{0, 0, static_cast<float>(hdrSceneScratch.native.texture.width),
                    static_cast<float>(hdrSceneScratch.native.texture.height)},
            Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndShaderMode();
    EndTextureMode();
    return CommitHdrScratch(sceneTarget);
}

unsigned int SectorMeshRenderer::AtmosphereGpuQuery(
        std::size_t pass,
        std::size_t querySlot,
        bool end) const
{
    const std::size_t queryIndex =
            (querySlot * AtmosphereGpuPassCount + pass) * 2
            + (end ? 1u : 0u);
    return atmosphereGpuQueries[queryIndex];
}

bool SectorMeshRenderer::EnsureAtmosphereGpuQueries()
{
    if (atmosphereGpuQueriesInitialized) return true;
    if (glQueryCounter == nullptr) return false;
    glGenQueries(
            static_cast<GLsizei>(atmosphereGpuQueries.size()),
            atmosphereGpuQueries.data());
    atmosphereGpuQueriesInitialized = atmosphereGpuQueries[0] != 0;
    return atmosphereGpuQueriesInitialized;
}

void SectorMeshRenderer::ShutdownAtmosphereGpuQueries()
{
    if (atmosphereGpuQueriesInitialized) {
        glDeleteQueries(
                static_cast<GLsizei>(atmosphereGpuQueries.size()),
                atmosphereGpuQueries.data());
    }
    atmosphereGpuQueries.fill(0);
    atmosphereGpuIssuedMasks.fill(0);
    atmosphereGpuFrameIndex = 0;
    atmosphereGpuSlot = 0;
    atmosphereGpuQueriesInitialized = false;
    atmosphereGpuActive = false;
}

void SectorMeshRenderer::BeginAtmosphereGpuFrame(bool enabled)
{
    atmosphereGpuActive = false;
    atmosphereGpuSlot = atmosphereGpuFrameIndex % AtmosphereGpuQueryLatency;
    const std::uint8_t issuedMask =
            atmosphereGpuIssuedMasks[atmosphereGpuSlot];
    if (issuedMask != 0 && atmosphereGpuQueriesInitialized) {
        bool ready = true;
        for (std::size_t pass = 0; pass < AtmosphereGpuPassCount; ++pass) {
            if ((issuedMask & (1u << pass)) == 0) continue;
            GLint available = GL_FALSE;
            glGetQueryObjectiv(
                    AtmosphereGpuQuery(pass, atmosphereGpuSlot, true),
                    GL_QUERY_RESULT_AVAILABLE,
                    &available);
            ready = ready && available == GL_TRUE;
        }
        if (ready) {
            double* values[AtmosphereGpuPassCount] = {
                    &atmosphereDiagnostics.causticsGpuMilliseconds,
                    &atmosphereDiagnostics.distanceFogGpuMilliseconds,
                    &atmosphereDiagnostics.analyticFogGpuMilliseconds,
                    &atmosphereDiagnostics.analyticShaftGpuMilliseconds,
                    &atmosphereDiagnostics.lightHaloGpuMilliseconds,
                    &atmosphereDiagnostics.dustGpuMilliseconds,
                    &atmosphereDiagnostics
                            .underwaterParticlesGpuMilliseconds};
            for (std::size_t pass = 0; pass < AtmosphereGpuPassCount; ++pass) {
                if ((issuedMask & (1u << pass)) == 0) continue;
                GLuint64 startNanoseconds = 0;
                GLuint64 endNanoseconds = 0;
                glGetQueryObjectui64v(
                        AtmosphereGpuQuery(pass, atmosphereGpuSlot, false),
                        GL_QUERY_RESULT,
                        &startNanoseconds);
                glGetQueryObjectui64v(
                        AtmosphereGpuQuery(pass, atmosphereGpuSlot, true),
                        GL_QUERY_RESULT,
                        &endNanoseconds);
                if (endNanoseconds >= startNanoseconds) {
                    const double sample = static_cast<double>(
                            endNanoseconds - startNanoseconds) / 1000000.0;
                    *values[pass] = *values[pass] <= 0.0
                            ? sample
                            : *values[pass] * 0.85 + sample * 0.15;
                }
            }
            atmosphereGpuIssuedMasks[atmosphereGpuSlot] = 0;
        }
    }
    if (enabled && EnsureAtmosphereGpuQueries()
            && atmosphereGpuIssuedMasks[atmosphereGpuSlot] == 0) {
        atmosphereGpuActive = true;
    }
    ++atmosphereGpuFrameIndex;
}

void SectorMeshRenderer::BeginAtmosphereGpuPass(std::size_t pass)
{
    if (!atmosphereGpuActive || pass >= AtmosphereGpuPassCount) return;
    glQueryCounter(
            AtmosphereGpuQuery(pass, atmosphereGpuSlot, false),
            GL_TIMESTAMP);
}

void SectorMeshRenderer::EndAtmosphereGpuPass(std::size_t pass)
{
    if (!atmosphereGpuActive || pass >= AtmosphereGpuPassCount) return;
    glQueryCounter(
            AtmosphereGpuQuery(pass, atmosphereGpuSlot, true),
            GL_TIMESTAMP);
    atmosphereGpuIssuedMasks[atmosphereGpuSlot] |=
            static_cast<std::uint8_t>(1u << pass);
}

void SectorMeshRenderer::RefreshAtmosphereDiagnostics(
        const SectorBillboardDynamicLightContext& dynamicLights)
{
    atmosphereDiagnostics.dynamicLightCount = dynamicLights.dynamicLightCount;
    atmosphereDiagnostics.analyticFogEligibleCount =
            analyticFogRenderer.EligibleVolumeCount();
    atmosphereDiagnostics.analyticFogActiveCount =
            analyticFogRenderer.ActiveVolumeCount();
    atmosphereDiagnostics.analyticFogScissorCoverage =
            analyticFogRenderer.ScissorCoverage();
    atmosphereDiagnostics.analyticShaftEligibleCount = analyticLightShaftRenderer.EligibleCount();
    atmosphereDiagnostics.analyticShaftActiveCount = analyticLightShaftRenderer.ActiveCount();
    atmosphereDiagnostics.analyticShaftScissorCoverage = analyticLightShaftRenderer.ScissorCoverage();
    atmosphereDiagnostics.analyticShaftDrawCallCount = analyticLightShaftRenderer.DrawCallCount();
    atmosphereDiagnostics.lightHaloEligibleCount = lightProxyRenderer.EligibleCount();
    atmosphereDiagnostics.lightHaloCount = lightProxyRenderer.HaloCount();
    atmosphereDiagnostics.lightHaloScissorCoverage = lightProxyRenderer.ScissorCoverage();
    atmosphereDiagnostics.lightHaloDrawCallCount = lightProxyRenderer.DrawCallCount();
    atmosphereDiagnostics.dustEligibleEmitterCount =
            lightDustRenderer.EligibleEmitterCount();
    atmosphereDiagnostics.dustActiveEmitterCount =
            lightDustRenderer.ActiveEmitterCount();
    atmosphereDiagnostics.dustVisibleParticleCount =
            lightDustRenderer.VisibleParticleCount();
    atmosphereDiagnostics.underwaterVisibleParticleCount =
            underwaterRenderer.VisibleParticleCount();
}

bool SectorMeshRenderer::ApplyTransparentSurfaces(
        engine::RenderTarget& sceneTarget,
        engine::AssetManager& assets,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting,
        const SectorTopologyFogSettings& fogSettings,
        const SectorUnderwaterRenderContext& underwater,
        bool collectGpuDiagnostics)
{
    preGlassLightEffectsRendered = false;
    preGlassShaftApplied = false;
    preGlassHaloApplied = false;
    if (!initialized) return false;

    // Halo and shaft GPU timings remain part of the atmosphere diagnostics even
    // though these two effects are composited before transparent windows.
    BeginAtmosphereGpuFrame(collectGpuDiagnostics);
    atmosphereGpuFramePrepared = true;
    // Collection is enabled for the next scene pass; diagnostics become available
    // after the initial warm-up frame, including command-line frame tracing.
    worldDiagnosticsEnabled = collectGpuDiagnostics;

    const bool visibleWindows = runtimeObjectWorld != nullptr
            && windowRenderer.PrepareVisibleWindows(*runtimeObjectWorld, camera,
                    static_cast<float>(sceneTarget.native.texture.width)
                            / std::max(1, sceneTarget.native.texture.height),
                    &visibilityResult, glassEnabled);
    const bool visibleLiquids = liquidRenderer.HasVisibleLiquids(
            &visibilityResult, camera.position);
    const SectorTopologyMap* map = doorLighting.mapForFallback;
    const SectorBillboardDynamicLightContext lightContext =
            BuildBillboardDynamicLightContext();
    BeginAtmosphereGpuPass(0);
    bool causticsApplied = false;
    if (map != nullptr
            && sceneTarget.descriptor.colorFormat
                    == engine::RenderTargetColorFormat::Rgba16Float
            && sceneTarget.actual.depth
                    == engine::RenderTargetDepthKind::SampleableTexture
            && EnsureHdrSceneScratch(sceneTarget)) {
        causticsApplied = underwaterRenderer.ApplyCaustics(
                sceneTarget.native,
                hdrSceneScratch.native,
                assets,
                camera,
                runtimeSeconds,
                underwater);
        if (causticsApplied && !CommitHdrScratch(sceneTarget)) {
            causticsApplied = false;
        }
    }
    EndAtmosphereGpuPass(0);
    const bool canRenderPreGlassEffects = (visibleWindows || visibleLiquids)
            && map != nullptr
            && sceneTarget.descriptor.colorFormat
                    == engine::RenderTargetColorFormat::Rgba16Float
            && sceneTarget.actual.depth
                    == engine::RenderTargetDepthKind::SampleableTexture
            && EnsureHdrSceneColorView(sceneTarget);
    if (canRenderPreGlassEffects) {
        worldProfiler.Begin(SectorWorldStage::PreGlass);
        SectorTopologyFogSettings effectFogSettings = fogSettings;
        if (NormalizeSectorTopologyFogSettings(fogSettings).mode
                == SectorTopologyFogMode::Distance) {
            // The later full-screen distance fog pass sees these pixels and
            // applies the attenuation once to effects and glass together.
            effectFogSettings.enabled = false;
        }
        const RuntimePortalVisibilityResult& lightingVisibility =
                dynamicLightState.LightingVisibility();
        BeginAtmosphereGpuPass(3);
        preGlassShaftApplied = analyticLightShaftRenderer.Apply(
                sceneTarget.native, hdrSceneColorView, effectFogSettings,
                camera, lightContext, lightAtmosphereSources,
                lightingVisibility, meshes.sectorReceiverBounds);
        EndAtmosphereGpuPass(3);
        BeginAtmosphereGpuPass(4);
        preGlassHaloApplied = lightProxyRenderer.Apply(
                sceneTarget.native, hdrSceneColorView, effectFogSettings,
                camera, lightContext, lightAtmosphereSources,
                lightingVisibility, meshes.sectorReceiverBounds);
        EndAtmosphereGpuPass(4);
        preGlassLightEffectsRendered = true;
        worldProfiler.End();
    }

    if (!visibleWindows && !visibleLiquids) {
        worldProfiler.diagnostics.panes = 0;
        worldProfiler.diagnostics.panesCulled = windowRenderer.ConsideredCount();
        worldProfiler.FinishFrame();
        atmosphereDiagnostics.world = worldProfiler.diagnostics;
        renderDebugText += " | transparents: idle";
        return causticsApplied || preGlassShaftApplied || preGlassHaloApplied;
    }

    bool refractionReady = visibleLiquids
            && sceneTarget.actual.depth
                    == engine::RenderTargetDepthKind::SampleableTexture
            && sceneTarget.native.depth.id != 0
            && EnsureHdrSceneScratch(sceneTarget)
            && EnsureHdrSceneColorView(sceneTarget);
    if (refractionReady) {
        rlDrawRenderBatchActive();
        refractionReady = BlitFramebufferColor(
                sceneTarget.native, hdrSceneScratch.native);
        if (refractionReady) SetTextureFilter(
                hdrSceneScratch.native.texture, TEXTURE_FILTER_BILINEAR);
    }

    SectorTopologyFogSettings materialFogSettings = fogSettings;
    if (NormalizeSectorTopologyFogSettings(fogSettings).mode
            == SectorTopologyFogMode::Distance) {
        materialFogSettings.enabled = false;
    }
    const SectorFogRenderContext fogContext =
            BuildSectorFogRenderContext(materialFogSettings, camera.position);
    const Vector2 viewportSize{
            static_cast<float>(sceneTarget.native.texture.width),
            static_cast<float>(sceneTarget.native.texture.height)};

    if (visibleLiquids) {
        BeginTextureMode(refractionReady ? hdrSceneColorView : sceneTarget.native);
        BeginMode3D(camera);
        SectorLiquidDrawContext liquidContext;
        liquidContext.assets = &assets;
        liquidContext.camera = camera;
        liquidContext.visibility = &visibilityResult;
        liquidContext.environment = &pbrEnvironment;
        liquidContext.pbr = pbrContributionSettings;
        if (map != nullptr) liquidContext.directionalLight = map->directionalLight;
        liquidContext.fog = fogContext;
        liquidContext.advancedTransmission = refractionReady;
        liquidContext.sceneColor = refractionReady
                ? &hdrSceneScratch.native.texture : nullptr;
        liquidContext.sceneDepth = refractionReady
                ? &sceneTarget.native.depth : nullptr;
        liquidContext.viewportSize = viewportSize;
        liquidContext.runtimeSeconds = runtimeSeconds;
        liquidContext.renderDebugText = &renderDebugText;
        liquidRenderer.Draw(liquidContext);
        EndMode3D();
        EndTextureMode();
    }

    if (refractionReady) {
        SetTextureFilter(hdrSceneScratch.native.texture, TEXTURE_FILTER_POINT);
    }

    if (visibleLiquids && !refractionReady) {
        if (!liquidRefractionFallbackLogged) {
            TraceLog(LOG_WARNING,
                    "LIQUID: scene sampling unavailable; using reflection/tint fallback");
            liquidRefractionFallbackLogged = true;
        }
        renderDebugText += " | liquid refraction fallback (sampleable depth or HDR scratch unavailable)";
    }

    if (visibleWindows) {
        BeginTextureMode(sceneTarget.native);
        BeginMode3D(camera);
        SectorWindowDrawContext windowContext;
        windowContext.profiler = &worldProfiler;
        windowContext.assets = &assets;
        windowContext.world = runtimeObjectWorld;
        windowContext.camera = camera;
        windowContext.visibility = &visibilityResult;
        windowContext.environment = &pbrEnvironment;
        windowContext.pbr = pbrContributionSettings;
        if (map != nullptr) windowContext.directionalLight = map->directionalLight;
        windowContext.fog = fogContext;
        windowContext.advancedTransmission = false;
        windowContext.viewportSize = viewportSize;
        windowContext.renderDebugText = &renderDebugText;
        windowRenderer.Draw(windowContext);
        EndMode3D();
        EndTextureMode();
    }
    worldProfiler.diagnostics.panes = visibleWindows ? windowRenderer.DrawnCount() : 0;
    worldProfiler.diagnostics.panesCulled = windowRenderer.ConsideredCount() - worldProfiler.diagnostics.panes;
    worldProfiler.FinishFrame();
    atmosphereDiagnostics.world = worldProfiler.diagnostics;
    return true;
}

bool SectorMeshRenderer::ApplyWorldAtmosphere(
        engine::RenderTarget& sceneTarget,
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorUnderwaterRenderContext& underwater,
        bool collectGpuDiagnostics)
{
    if (!atmosphereGpuFramePrepared) {
        BeginAtmosphereGpuFrame(collectGpuDiagnostics);
    }
    atmosphereGpuFramePrepared = false;
    const bool skipPostGlassLightEffects = preGlassLightEffectsRendered;
    const bool earlyShaftApplied = preGlassShaftApplied;
    const bool earlyHaloApplied = preGlassHaloApplied;
    preGlassLightEffectsRendered = false;
    preGlassShaftApplied = false;
    preGlassHaloApplied = false;
    if (sceneTarget.descriptor.colorFormat
                    != engine::RenderTargetColorFormat::Rgba16Float
            || sceneTarget.actual.depth
                    != engine::RenderTargetDepthKind::SampleableTexture
            || !EnsureHdrSceneScratch(sceneTarget)) {
        RefreshAtmosphereDiagnostics(SectorBillboardDynamicLightContext{});
        return earlyShaftApplied || earlyHaloApplied;
    }
    RenderTexture2D& nativeScene = sceneTarget.native;
    const SectorBillboardDynamicLightContext dynamicLightContext =
            BuildBillboardDynamicLightContext();
    bool atmosphereFailed = false;
    BeginAtmosphereGpuPass(1);
    const bool distanceFogApplied = distanceFogRenderer.Apply(
            nativeScene, hdrSceneScratch.native, map.fogSettings, camera);
    if (distanceFogApplied && !CommitHdrScratch(sceneTarget)) atmosphereFailed = true;
    EndAtmosphereGpuPass(1);
    if (atmosphereFailed) {
        RefreshAtmosphereDiagnostics(dynamicLightContext);
        return false;
    }

    BeginAtmosphereGpuPass(2);
    bool analyticFogApplied = false;
    if (EnsureHdrSceneColorView(sceneTarget)) {
        analyticFogApplied = analyticFogRenderer.Apply(
                nativeScene,
                hdrSceneColorView,
                map,
                camera,
                runtimeSeconds,
                objectLightProbes,
                visibilityResult);
    }
    EndAtmosphereGpuPass(2);

    if (!skipPostGlassLightEffects) BeginAtmosphereGpuPass(3);
    bool analyticShaftApplied = earlyShaftApplied;
    if (!skipPostGlassLightEffects && EnsureHdrSceneColorView(sceneTarget)) {
        analyticShaftApplied = analyticLightShaftRenderer.Apply(
                nativeScene, hdrSceneColorView, map.fogSettings,
                camera, dynamicLightContext, lightAtmosphereSources,
                dynamicLightState.LightingVisibility(),
                meshes.sectorReceiverBounds);
    }
    if (!skipPostGlassLightEffects) EndAtmosphereGpuPass(3);

    if (!skipPostGlassLightEffects) BeginAtmosphereGpuPass(4);
    bool lightHaloApplied = earlyHaloApplied;
    if (!skipPostGlassLightEffects && EnsureHdrSceneColorView(sceneTarget)) {
        lightHaloApplied = lightProxyRenderer.Apply(
                nativeScene, hdrSceneColorView, map.fogSettings,
                camera, dynamicLightContext, lightAtmosphereSources,
                dynamicLightState.LightingVisibility(),
                meshes.sectorReceiverBounds);
    }
    if (!skipPostGlassLightEffects) EndAtmosphereGpuPass(4);

    BeginAtmosphereGpuPass(5);
    const bool lightDustApplied = lightDustRenderer.Apply(
            nativeScene,
            hdrSceneScratch.native,
            map,
            camera,
            runtimeSeconds,
            objectLightProbes,
            dynamicLightContext,
            lightAtmosphereSources,
            visibilityResult,
            meshes.sectorReceiverBounds);
    EndAtmosphereGpuPass(5);
    BeginAtmosphereGpuPass(6);
    const bool underwaterParticlesApplied = underwaterRenderer.DrawParticles(
            nativeScene,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            camera,
            runtimeSeconds,
            underwater);
    EndAtmosphereGpuPass(6);
    RefreshAtmosphereDiagnostics(dynamicLightContext);
    return distanceFogApplied || analyticFogApplied || analyticShaftApplied
            || lightHaloApplied || lightDustApplied
            || underwaterParticlesApplied;
}

bool SectorMeshRenderer::ApplyHdrBloom(
        engine::RenderTarget& sceneTarget,
        const engine::HdrBloomSettings& settings,
        bool presentFromScratch)
{
    hdrPresentationSource = nullptr;
    if (!initialized || !EnsureHdrSceneScratch(sceneTarget)
            || !bloomRenderer.Apply(sceneTarget, hdrSceneScratch, settings)) {
        return false;
    }
    if (presentFromScratch) {
        hdrPresentationSource = &hdrSceneScratch;
        return true;
    }
    return CommitHdrScratch(sceneTarget);
}

bool SectorMeshRenderer::PreparePostBloomWorldOverlays(
        engine::RenderTarget& sceneTarget,
        bool overlayRequested)
{
    const engine::HdrPostProcessOverlayRoute route =
            engine::ResolveHdrPostProcessOverlayRoute(
                    overlayRequested,
                    hdrPresentationSource == &hdrSceneScratch,
                    bloomRenderer.DebugSource() != nullptr);
    if (route == engine::HdrPostProcessOverlayRoute::Skip) return false;
    if (route
            == engine::HdrPostProcessOverlayRoute::CommitScratchThenDrawSceneTarget) {
        if (!CommitHdrScratch(sceneTarget)) return false;
        hdrPresentationSource = nullptr;
    }
    return true;
}

SectorViewPose SectorMeshRenderer::Pose() const
{
    return RendererPose();
}

SectorViewPose SectorMeshRenderer::RendererPose() const
{
    return SectorViewPose{position, yawRadians, pitchRadians, rollRadians};
}

void SectorMeshRenderer::ApplyPose(const SectorViewPose& pose)
{
    ApplyRendererPose(pose);
}

void SectorMeshRenderer::ApplyRendererPose(
        const SectorViewPose& pose,
        bool refreshVisibility)
{
    position = pose.position;
    yawRadians = pose.yawRadians;
    pitchRadians = pose.pitchRadians;
    rollRadians = pose.rollRadians;
    UpdateCamera();
    if (refreshVisibility) {
        UpdateVisibilityDebug();
    }
}

void SectorMeshRenderer::SetVerticalFovDegrees(float value)
{
    if (!std::isfinite(value)) {
        return;
    }
    verticalFovDegrees = std::clamp(value, 1.0f, 179.0f);
    camera.fovy = verticalFovDegrees;
}

void SectorMeshRenderer::RefreshDynamicLightSources(const SectorTopologyMap& map)
{
    dynamicLightState.RebuildSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
    RebuildSectorStaticSpecularLights(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            meshes.sectorReceiverBounds,
            staticSpecularLightState);
    RefreshBakedDataStatus(map);
    BuildSectorLightAtmosphereSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            lightAtmosphereSources);
    analyticFogRenderer.Reserve(map.compiledLocalFogVolumes.size());
    analyticLightShaftRenderer.Reserve(lightAtmosphereSources.size());
    lightProxyRenderer.Reserve(lightAtmosphereSources.size());
    UpdateVisibilityDebug();
}

void SectorMeshRenderer::RefreshBakedDataStatus(const SectorTopologyMap& map)
{
    RefreshBakedDataStatus(map, ComputeSectorLightmapSourceHash(map));
}

void SectorMeshRenderer::RefreshBakedDataStatus(
        const SectorTopologyMap& map,
        const std::string& currentSourceHash)
{
    const SectorLightmapStatus currentLightmapStatus =
            GetSectorLightmapStatus(map, currentSourceHash);
    lightmapStatus = static_cast<int>(currentLightmapStatus);
    surfaceLightmapBakeCurrent =
            currentLightmapStatus == SectorLightmapStatus::Valid
            && !lightmapTextures.empty();
    objectProbeBakeCurrent =
            GetSectorBakedObjectLightProbeStatus(map, currentSourceHash)
                    == SectorLightmapStatus::Valid;
}

void SectorMeshRenderer::BeginStaticObjectAdjustmentBakedDataStale()
{
    if (!staticObjectAdjustmentBakedDataActive) {
        staticObjectAdjustmentOriginalLightmapStatus = lightmapStatus;
        staticObjectAdjustmentOriginalSurfaceLightmapCurrent =
                surfaceLightmapBakeCurrent;
        staticObjectAdjustmentOriginalObjectProbeCurrent =
                objectProbeBakeCurrent;

        staticObjectAdjustmentBakedDataActive = true;
    }
    if (static_cast<SectorLightmapStatus>(lightmapStatus)
            == SectorLightmapStatus::Valid) {
        lightmapStatus = static_cast<int>(SectorLightmapStatus::Stale);
    }
    surfaceLightmapBakeCurrent = false;
    objectProbeBakeCurrent = false;
    runtimeReflections.Invalidate(pbrEnvironment);
}

void SectorMeshRenderer::FinishStaticObjectAdjustmentBakedData(bool restore)
{
    if (!staticObjectAdjustmentBakedDataActive) return;
    if (restore) {
        lightmapStatus = staticObjectAdjustmentOriginalLightmapStatus;
        surfaceLightmapBakeCurrent =
                staticObjectAdjustmentOriginalSurfaceLightmapCurrent;
        objectProbeBakeCurrent =
                staticObjectAdjustmentOriginalObjectProbeCurrent;

    }
    staticObjectAdjustmentBakedDataActive = false;
}

void SectorMeshRenderer::UpdateVisibilityDebug(
        int preferredStartSectorId,
        float visibilitySeedRadiusWorld,
        bool validateEyeY,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers,
        engine::World* runtimeObjectWorld)
{
    if (dynamicPortalBlockers) {
        bool changed=pbrEnvironment.blockers.size()!=dynamicPortalBlockers->size();
        for (std::size_t i=0; !changed && i<dynamicPortalBlockers->size(); ++i) {
            const auto& a=pbrEnvironment.blockers[i]; const auto& b=(*dynamicPortalBlockers)[i];
            changed=a.lineDefId!=b.lineDefId || a.blocksPortal!=b.blocksPortal;
        }
        if (changed) {pbrEnvironment.blockers=*dynamicPortalBlockers;runtimeReflections.Invalidate(pbrEnvironment);}
    }

    if (!visibilityGraphValid) {
        visibilityResult = RuntimePortalVisibilityResult{};
        visibilityResult.startSectorId = -1;
        visibilityResult.fallbackDrawAll = true;
        visibilityResult.status = "visibility graph unavailable; fallback draw all";
    } else {
        visibilityResult = ComputeRuntimeSectorVisibilityFromView(
                visibilityGraph,
                visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
                Vector2{camera.position.x, camera.position.z},
                PreviewYawForwardXZ(yawRadians),
                VisibilityDebugHorizontalFovRadians(camera, pitchRadians),
                preferredStartSectorId,
                0,
                visibilitySeedRadiusWorld,
                camera.position.y,
                validateEyeY,
                dynamicPortalBlockers,
                &visibilityScratch);
    }
    portalVisibilityDebugText = FormatRuntimePortalVisibilityDebugText(visibilityResult);
    visibilityDebugText = portalVisibilityDebugText;
    const size_t visibleDrawRecordCount =
            CountSectorMeshDrawRecordsForVisibility(meshes.sectorDrawRecords, visibilityResult);
    dynamicLightState.UpdateSelection(
            visibilityResult,
            visibilityResult.validStartSector
                    ? visibilityResult.startSectorId
                    : preferredStartSectorId,
            meshes.sectorReceiverBounds,
            runtimeObjectWorld,
            visibilityGraphValid ? &visibilityGraph : nullptr,
            dynamicPortalBlockers);
    renderDebugText = "draw records: "
            + std::to_string(visibleDrawRecordCount)
            + " / "
            + std::to_string(meshes.sectorDrawRecords.size());
    renderDebugText += " | "
            + FormatDynamicLightDebugText(
                    dynamicLightingEnabled,
                    dynamicLightState.SelectedLights().size(),
                    dynamicLightState.CandidateCount(),
                    dynamicLightState.SourceCount(),
                    dynamicLightState.SelectedLightKeys());
    const SectorDynamicLightSelectionStats& selectionStats =
            dynamicLightState.SelectionStats();
    renderDebugText += " | lighting component sectors "
            + std::to_string(selectionStats.reachableSectorCount)
            + (selectionStats.reachabilityCacheHit ? " cached" : " rebuilt")
            + " start "
            + std::to_string(selectionStats.lightingStartSectorId)
            + " blockers "
            + std::to_string(selectionStats.dynamicPortalBlockerCount)
            + (selectionStats.cameraVisibilityFallback
                    ? " camera-fallback"
                    : "")
            + " | visible receiver refs "
            + std::to_string(selectionStats.visibleReceiverLightReferences)
            + " total / "
            + std::to_string(selectionStats.maxVisibleReceiverLights)
            + " max across "
            + std::to_string(selectionStats.visibleReceiverCount)
            + " bounds";
    renderDebugText += " | "
            + FormatDynamicSpotLightShadowDebugText(
                    dynamicLightState.ShadowCasters().size(),
                    CountDynamicSpotLightShadowCandidates(dynamicLightState.SelectedLights()),
                    dynamicLightState.ShadowSlotBudget(),
                    dynamicLightState.ShadowCasters(),
                    dynamicLightState.SelectedLights());
    AppendBillboardRenderDebugText(renderDebugText, billboardRenderer.DebugText());
    visibilityDebugText += " | " + renderDebugText;
}

float SectorMeshRenderer::AssetProgress(engine::AssetManager& assets) const
{
    return RendererAssetProgress(assets);
}

float SectorMeshRenderer::RendererAssetProgress(engine::AssetManager& assets) const
{
    return engine::IsNull(assetScope) ? 1.0f : assets.GetScopeProgress(assetScope);
}

const char* SectorMeshRenderer::LightmapStatusText() const
{
    return RendererLightmapStatusText();
}

const char* SectorMeshRenderer::RendererLightmapStatusText() const
{
    return SectorLightmapStatusText(static_cast<SectorLightmapStatus>(lightmapStatus));
}

engine::TextureHandle SectorMeshRenderer::TextureForId(const std::string& materialId) const
{
    const auto it = textureHandlesById.find(materialId);
    if (it == textureHandlesById.end()) {
        return engine::NullTextureHandle();
    }

    return it->second;
}

engine::TextureHandle SectorMeshRenderer::NormalTextureForId(const std::string& materialId) const
{
    const auto it = normalTextureHandlesById.find(materialId);
    if (it == normalTextureHandlesById.end()) {
        return engine::NullTextureHandle();
    }
    return it->second;
}

engine::TextureHandle SectorMeshRenderer::PropertyTextureForId(
        const std::string& materialId) const
{
    const auto it = propertyTextureHandlesById.find(materialId);
    return it == propertyTextureHandlesById.end()
            ? engine::NullTextureHandle()
            : it->second;
}

SectorMaterialPropertyMapKind SectorMeshRenderer::PropertyMapKindForId(
        const std::string& materialId) const
{
    const auto it = propertyMapKindsById.find(materialId);
    return it == propertyMapKindsById.end()
            ? SectorMaterialPropertyMapKind::None
            : it->second;
}

const Texture2D* SectorMeshRenderer::ResolveShadowCasterTexture(
        void* userData,
        engine::AssetManager& assets,
        const std::string& materialId)
{
    const SectorMeshRenderer* preview = static_cast<const SectorMeshRenderer*>(userData);
    if (preview == nullptr) {
        return nullptr;
    }
    return assets.GetTexture(preview->TextureForId(materialId));
}

SectorDoorResolvedMaterial SectorMeshRenderer::ResolveDoorMaterial(
        void* userData,
        engine::AssetManager& assets,
        const std::string& materialId)
{
    const SectorMeshRenderer* preview = static_cast<const SectorMeshRenderer*>(
            userData);
    if (preview == nullptr) {
        return {};
    }

    SectorDoorResolvedMaterial result;
    result.albedo = assets.GetTexture(preview->TextureForId(materialId));
    result.normal = assets.GetTexture(preview->NormalTextureForId(materialId));
    result.properties = assets.GetTexture(
            preview->PropertyTextureForId(materialId));
    if (result.properties != nullptr) {
        result.propertyMapKind = preview->PropertyMapKindForId(materialId);
    }
    const auto normalStrength = preview->normalStrengthById.find(materialId);
    if (normalStrength != preview->normalStrengthById.end()) {
        result.normalStrength = normalStrength->second;
    }
    const auto metallic = preview->metallicFactorById.find(materialId);
    if (metallic != preview->metallicFactorById.end()) {
        result.metallicFactor = metallic->second;
    }
    const auto roughness = preview->roughnessFactorById.find(materialId);
    if (roughness != preview->roughnessFactorById.end()) {
        result.roughnessFactor = roughness->second;
    }
    return result;
}

void SectorMeshRenderer::UpdateCamera()
{
    const SectorViewPose pose{position, yawRadians, pitchRadians, rollRadians};
    const Vector3 look = SectorViewForward(pose);

    camera.position = position;
    camera.target = Vector3Add(position, look);
    camera.up = SectorViewUp(pose);
}

} // namespace game
