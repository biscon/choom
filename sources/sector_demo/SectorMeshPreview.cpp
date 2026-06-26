#include "sector_demo/SectorMeshPreview.h"

#include "engine/assets/TextureLoadFlags.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMeshBuilder.h"
#include "sector_demo/SectorSkyCylinder.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace game {

namespace {

constexpr bool BloomEnabled = true;
constexpr float BloomStrength = 0.5f;
constexpr float BloomLdrIntensityScale = 10.0f;
constexpr int BloomIterations = 3;
constexpr int BloomDownsample = 4;

const char* SectorLightmapVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec4 vertexTangent;
in vec4 vertexColor;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec2 fragTexCoord2;
out vec2 fragDecalUv;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragTexCoord2 = vertexTexCoord2;
    fragDecalUv = vertexTangent.xy;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* SectorLightmapFs = R"(
#version 330
in vec2 fragTexCoord;
in vec2 fragTexCoord2;
in vec2 fragDecalUv;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D decalTexture;
uniform float useLightmap;
uniform float useBakedAmbientOcclusion;
uniform int hasLightmap;
uniform int alphaTest;
uniform float alphaCutoff;
uniform int hasDecal;
uniform float decalOpacity;
uniform int decalEmissive;
uniform vec3 decalTint;

out vec4 finalColor;

void main()
{
    vec4 baseColor = texture(texture0, fragTexCoord);
    if (alphaTest != 0 && baseColor.a < alphaCutoff) {
        discard;
    }
    vec3 surfaceRgb = baseColor.rgb;
    vec3 emissiveDecalRgb = vec3(0.0);
    float emissiveDecalAlpha = 0.0;
    if (hasDecal != 0) {
        float decalMask =
            fragDecalUv.x >= 0.0 && fragDecalUv.x <= 1.0 &&
            fragDecalUv.y >= 0.0 && fragDecalUv.y <= 1.0
                ? 1.0
                : 0.0;
        vec4 decalColor = texture(decalTexture, fragDecalUv);
        float decalAlpha = decalColor.a * decalOpacity * decalMask;
        vec3 decalRgb = decalColor.rgb * decalTint;
        if (decalEmissive != 0) {
            emissiveDecalRgb = decalRgb;
            emissiveDecalAlpha = decalAlpha;
        } else {
            surfaceRgb = mix(baseColor.rgb, decalRgb, decalAlpha);
        }
    }
    vec4 bakedSample = (useLightmap > 0.5 && hasLightmap != 0) ? texture(texture1, fragTexCoord2) : vec4(0.0, 0.0, 0.0, 1.0);
    float aoFactor = (useBakedAmbientOcclusion > 0.5 && hasLightmap != 0) ? bakedSample.a : 1.0;
    vec3 ambient = fragColor.rgb * aoFactor;
    vec3 bakedDirect = bakedSample.rgb;
    vec3 lighting = clamp(ambient + bakedDirect, 0.0, 1.0);
    vec3 litRgb = surfaceRgb * lighting;
    finalColor = vec4(mix(litRgb, emissiveDecalRgb, emissiveDecalAlpha), baseColor.a * fragColor.a);
}
)";

const char* SectorBloomSourceFs = R"(
#version 330
in vec2 fragTexCoord;
in vec2 fragTexCoord2;
in vec2 fragDecalUv;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D decalTexture;
uniform int hasDecal;
uniform float decalOpacity;
uniform int decalEmissive;
uniform vec3 decalTint;
uniform float decalBloomIntensity;

out vec4 finalColor;

void main()
{
    if (hasDecal == 0 || decalEmissive == 0) {
        finalColor = vec4(0.0);
        return;
    }

    float decalMask =
        fragDecalUv.x >= 0.0 && fragDecalUv.x <= 1.0 &&
        fragDecalUv.y >= 0.0 && fragDecalUv.y <= 1.0
            ? 1.0
            : 0.0;
    vec4 decalColor = texture(decalTexture, fragDecalUv);
    float alpha = decalColor.a * decalOpacity * decalMask;
    if (alpha <= 0.0) {
        discard;
    }
    vec3 rgb = decalColor.rgb * decalTint * alpha * (decalBloomIntensity / 10.0);
    finalColor = vec4(rgb, 1.0);
}
)";

const char* BloomBlurFs = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texelSize;
uniform vec2 direction;

out vec4 finalColor;

void main()
{
    vec2 offset = direction * texelSize;
    vec4 color = texture(texture0, fragTexCoord) * 0.227027;
    color += texture(texture0, fragTexCoord + offset * 1.384615) * 0.316216;
    color += texture(texture0, fragTexCoord - offset * 1.384615) * 0.316216;
    color += texture(texture0, fragTexCoord + offset * 3.230769) * 0.070270;
    color += texture(texture0, fragTexCoord - offset * 3.230769) * 0.070270;
    finalColor = color * fragColor;
}
)";

const char* BloomCompositeFs = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform float bloomStrength;

out vec4 finalColor;

void main()
{
    vec4 scene = texture(texture0, fragTexCoord);
    vec3 bloom = texture(bloomTexture, fragTexCoord).rgb;
    finalColor = vec4(clamp(scene.rgb + bloom * bloomStrength, 0.0, 1.0), scene.a) * fragColor;
}
)";

Rectangle FullTextureSrcRect(const Texture2D& texture)
{
    return Rectangle{
            0.5f,
            0.5f,
            static_cast<float>(texture.width) - 1.0f,
            -static_cast<float>(texture.height) + 1.0f
    };
}

Rectangle FullTextureDstRect(const Texture2D& texture)
{
    return Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(texture.width),
            static_cast<float>(texture.height)
    };
}

bool StartsWith(const std::string& value, const char* prefix)
{
    const std::string prefixString(prefix);
    return value.compare(0, prefixString.size(), prefixString) == 0;
}

std::vector<std::string> SortedTopologyTextureIds(const SectorTopologyMap& map)
{
    std::vector<std::string> ids;
    ids.reserve(map.texturesById.size());
    for (const auto& texture : map.texturesById) {
        ids.push_back(texture.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool LoadPreviewMaterial(
        Material& material,
        Texture2D& defaultMaterialTexture,
        bool& materialLoaded,
        int& useLightmapLoc,
        int& useBakedAmbientOcclusionLoc,
        int& hasLightmapLoc,
        int& alphaTestLoc,
        int& alphaCutoffLoc,
        int& hasDecalLoc,
        int& decalOpacityLoc,
        int& decalEmissiveLoc,
        int& decalTintLoc,
        std::string& error)
{
    material = LoadMaterialDefault();
    Shader shader = LoadShaderFromMemory(SectorLightmapVs, SectorLightmapFs);
    if (shader.id == 0) {
        UnloadMaterial(material);
        material = Material{};
        error = "Preview failed: could not load sector lightmap shader";
        return false;
    }
    material.shader = shader;
    material.shader.locs[SHADER_LOC_VERTEX_TANGENT] = GetShaderLocationAttrib(material.shader, "vertexTangent");
    material.shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(material.shader, "texture0");
    material.shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(material.shader, "texture1");
    material.shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(material.shader, "decalTexture");
    useLightmapLoc = GetShaderLocation(material.shader, "useLightmap");
    useBakedAmbientOcclusionLoc = GetShaderLocation(material.shader, "useBakedAmbientOcclusion");
    hasLightmapLoc = GetShaderLocation(material.shader, "hasLightmap");
    alphaTestLoc = GetShaderLocation(material.shader, "alphaTest");
    alphaCutoffLoc = GetShaderLocation(material.shader, "alphaCutoff");
    hasDecalLoc = GetShaderLocation(material.shader, "hasDecal");
    decalOpacityLoc = GetShaderLocation(material.shader, "decalOpacity");
    decalEmissiveLoc = GetShaderLocation(material.shader, "decalEmissive");
    decalTintLoc = GetShaderLocation(material.shader, "decalTint");
    defaultMaterialTexture = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    materialLoaded = true;
    return true;
}

bool LoadBloomSourceMaterial(
        Material& material,
        Texture2D& defaultMaterialTexture,
        bool& materialLoaded,
        int& hasDecalLoc,
        int& decalOpacityLoc,
        int& decalEmissiveLoc,
        int& decalTintLoc,
        int& decalBloomIntensityLoc)
{
    material = LoadMaterialDefault();
    Shader shader = LoadShaderFromMemory(SectorLightmapVs, SectorBloomSourceFs);
    if (shader.id == 0) {
        UnloadMaterial(material);
        material = Material{};
        return false;
    }
    material.shader = shader;
    material.shader.locs[SHADER_LOC_VERTEX_TANGENT] = GetShaderLocationAttrib(material.shader, "vertexTangent");
    material.shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(material.shader, "texture0");
    material.shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(material.shader, "texture1");
    material.shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(material.shader, "decalTexture");
    hasDecalLoc = GetShaderLocation(material.shader, "hasDecal");
    decalOpacityLoc = GetShaderLocation(material.shader, "decalOpacity");
    decalEmissiveLoc = GetShaderLocation(material.shader, "decalEmissive");
    decalTintLoc = GetShaderLocation(material.shader, "decalTint");
    decalBloomIntensityLoc = GetShaderLocation(material.shader, "decalBloomIntensity");
    defaultMaterialTexture = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    materialLoaded = true;
    return true;
}

bool ComputeGeometryBounds(const SectorGeneratedGeometry& geometry, Vector3& outMin, Vector3& outMax)
{
    outMin = Vector3{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
    outMax = Vector3{
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max()};
    bool found = false;
    for (const SectorGeneratedSurface& surface : geometry.surfaces) {
        for (const SectorGeneratedVertex& vertex : surface.vertices) {
            outMin.x = std::min(outMin.x, vertex.position.x);
            outMin.y = std::min(outMin.y, vertex.position.y);
            outMin.z = std::min(outMin.z, vertex.position.z);
            outMax.x = std::max(outMax.x, vertex.position.x);
            outMax.y = std::max(outMax.y, vertex.position.y);
            outMax.z = std::max(outMax.z, vertex.position.z);
            found = true;
        }
    }
    return found;
}

Mesh CreateSkyCylinderMesh(const SectorSkyCylinderMeshData& data)
{
    Mesh mesh = {};
    if (data.positions.empty()
            || data.positions.size() != data.normals.size()
            || data.positions.size() != data.uvs.size()
            || data.positions.size() > static_cast<size_t>(std::numeric_limits<int>::max())
            || data.indices.empty()
            || data.indices.size() % 3u != 0u
            || data.indices.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return mesh;
    }

    mesh.vertexCount = static_cast<int>(data.positions.size());
    mesh.triangleCount = static_cast<int>(data.indices.size() / 3u);
    mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(static_cast<unsigned int>(data.indices.size() * sizeof(unsigned short))));

    if (mesh.vertices == nullptr
            || mesh.normals == nullptr
            || mesh.texcoords == nullptr
            || mesh.indices == nullptr) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to allocate sky cylinder mesh data\n");
        UnloadMesh(mesh);
        return Mesh{};
    }

    for (int i = 0; i < mesh.vertexCount; ++i) {
        const size_t index = static_cast<size_t>(i);
        mesh.vertices[i * 3 + 0] = data.positions[index].x;
        mesh.vertices[i * 3 + 1] = data.positions[index].y;
        mesh.vertices[i * 3 + 2] = data.positions[index].z;
        mesh.normals[i * 3 + 0] = data.normals[index].x;
        mesh.normals[i * 3 + 1] = data.normals[index].y;
        mesh.normals[i * 3 + 2] = data.normals[index].z;
        mesh.texcoords[i * 2 + 0] = data.uvs[index].x;
        mesh.texcoords[i * 2 + 1] = data.uvs[index].y;
    }

    for (size_t i = 0; i < data.indices.size(); ++i) {
        mesh.indices[i] = data.indices[i];
    }

    UploadMesh(&mesh, false);
    return mesh;
}

} // namespace

bool SectorMeshPreview::Rebuild(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const char* scopeName,
        std::string& error)
{
    return RebuildRendererResources(assets, map, scopeName, error);
}

bool SectorMeshPreview::RebuildRendererResources(
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

    if (map.texturesById.empty()) {
        error = "Preview failed: missing topology texture table";
        return false;
    }

    if (!BuildSectorGeneratedGeometry(map, generatedGeometry, &error)) {
        error = error.empty()
                ? "Preview failed: topology generated no geometry"
                : "Preview failed: " + error;
        generatedGeometry = {};
        return false;
    }

    assetScope = assets.CreateScope(scopeName == nullptr ? "sector_mesh_preview" : scopeName);
    if (engine::IsNull(assetScope)) {
        generatedGeometry = {};
        error = "Preview failed: could not create asset scope";
        return false;
    }

    for (const std::string& textureId : SortedTopologyTextureIds(map)) {
        const auto it = map.texturesById.find(textureId);
        if (it == map.texturesById.end()) {
            continue;
        }

        const SectorTextureDefinition& texture = it->second;
        const std::string resolvedPath = ResolveAssetPath(texture.path);
        engine::TextureHandle handle = assets.RequestTexture(
                assetScope,
                texture.id.c_str(),
                resolvedPath.c_str(),
                SectorTextureLoadFlags(texture.filter));
        textureHandlesById.emplace(texture.id, handle);
    }

    if (ShouldRenderSkyCylinder(map)) {
        const SectorTopologySkySettings skySettings = NormalizeSectorTopologySkySettings(map.skySettings);
        const SectorTextureDefinition* skyTexture = FindSkyTexture(map);
        skyTextureHandle = skyTexture == nullptr
                ? engine::NullTextureHandle()
                : TextureForId(skyTexture->id);
        skyYawOffsetDegrees = skySettings.yawOffsetDegrees;
        skyTopCapColor = skySettings.topColor;
        const SectorSkyCylinderMeshData skyData = BuildSkyCylinderMeshData(
                kDefaultSkyCylinderSegments,
                kDefaultSkyCylinderRadius,
                kDefaultSkyCylinderHeight,
                skySettings.verticalOffset,
                skySettings.verticalScale);
        const SectorSkyCylinderMeshData skyTopCapData = BuildSkyCylinderTopCapMeshData();
        skyCylinderMesh = CreateSkyCylinderMesh(skyData);
        skyTopCapMesh = CreateSkyCylinderMesh(skyTopCapData);
        if (skyCylinderMesh.vertexCount > 0 && skyTopCapMesh.vertexCount > 0) {
            skyMaterial = LoadMaterialDefault();
            skyDefaultMaterialTexture = skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
            skyMaterialLoaded = true;
        } else {
            UnloadSkyCylinderMesh();
            skyTextureHandle = engine::NullTextureHandle();
        }
    }

    SectorLightmapLayout lightmapLayout;
    const SectorLightmapStatus status = GetSectorLightmapStatus(map);
    lightmapStatus = static_cast<int>(status);
    const bool useLightmapLayout = status == SectorLightmapStatus::Valid
            && BuildSectorLightmapLayout(map, lightmapLayout, error);
    if (status == SectorLightmapStatus::Valid && !useLightmapLayout) {
        std::fprintf(stderr, "[SectorDemo WARNING] %s\n", error.c_str());
        error.clear();
    }

    if (useLightmapLayout) {
        const std::string resolvedPath = ResolveAssetPath(map.bakedLightmap.path);
        lightmapTexture = assets.RequestTexture(
                assetScope,
                "sector_lightmap_atlas",
                resolvedPath.c_str(),
                engine::TextureLoad_BilinearFilter);
    }

    std::string meshError;
    meshes = BuildSectorMeshes(map, useLightmapLayout ? &lightmapLayout : nullptr, &meshError);
    if (meshes.batches.empty()) {
        Shutdown(assets);
        error = meshError.empty()
                ? "Preview failed: topology mesh builder produced no batches"
                : "Preview failed: " + meshError;
        return false;
    }

    if (!LoadPreviewMaterial(
                material,
                defaultMaterialTexture,
                materialLoaded,
                useLightmapLoc,
                useBakedAmbientOcclusionLoc,
                hasLightmapLoc,
                alphaTestLoc,
                alphaCutoffLoc,
                hasDecalLoc,
                decalOpacityLoc,
                decalEmissiveLoc,
                decalTintLoc,
                error)) {
        Shutdown(assets);
        return false;
    }

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
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateCamera();

    initialized = true;
    return true;
}

void SectorMeshPreview::Shutdown(engine::AssetManager& assets)
{
    ShutdownRendererResources(assets);
}

void SectorMeshPreview::ShutdownRendererResources(engine::AssetManager& assets)
{
    generatedGeometry = {};
    if (!initialized
            && engine::IsNull(assetScope)
            && meshes.batches.empty()
            && !materialLoaded
            && skyCylinderMesh.vertexCount <= 0
            && skyTopCapMesh.vertexCount <= 0
            && !skyMaterialLoaded) {
        return;
    }

    UnloadBloomResources();
    UnloadSkyCylinderMesh();
    UnloadSectorMeshes(meshes);
    textureHandlesById.clear();
    lightmapTexture = engine::NullTextureHandle();
    skyTextureHandle = engine::NullTextureHandle();
    skyYawOffsetDegrees = 0.0f;
    skyTopCapColor = DefaultSectorTopologySkySettings().topColor;
    sectorCount = 0;

    if (materialLoaded) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = defaultMaterialTexture;
        material.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
        material.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
        UnloadMaterial(material);
        material = Material{};
        defaultMaterialTexture = Texture2D{};
        materialLoaded = false;
    }

    if (!engine::IsNull(assetScope)) {
        assets.UnloadScope(assetScope);
        assetScope = engine::NullAssetScopeHandle();
    }

    initialized = false;
}

void SectorMeshPreview::Render(engine::AssetManager& assets, bool useBakedAmbientOcclusion)
{
    DrawScene(assets, useBakedAmbientOcclusion);
}

void SectorMeshPreview::DrawScene(engine::AssetManager& assets, bool useBakedAmbientOcclusion)
{
    if (!initialized) {
        return;
    }

    BeginMode3D(camera);
    const Texture2D* skyTexture = assets.GetTexture(skyTextureHandle);
    if (skyTexture != nullptr
            && skyCylinderMesh.vertexCount > 0
            && skyTopCapMesh.vertexCount > 0
            && skyMaterialLoaded) {
        DrawSkyCylinder(*skyTexture);
    }

    const Texture2D* lightmap = assets.GetTexture(lightmapTexture);
    float useLightmap = lightmap != nullptr ? 1.0f : 0.0f;
    float useAo = useBakedAmbientOcclusion ? 1.0f : 0.0f;
    material.maps[MATERIAL_MAP_SPECULAR].texture = (lightmap != nullptr)
            ? *lightmap
            : Texture2D{};
    if (useLightmapLoc >= 0) {
        SetShaderValue(material.shader, useLightmapLoc, &useLightmap, SHADER_UNIFORM_FLOAT);
    }
    if (useBakedAmbientOcclusionLoc >= 0) {
        SetShaderValue(material.shader, useBakedAmbientOcclusionLoc, &useAo, SHADER_UNIFORM_FLOAT);
    }
    for (const SectorMeshBatch& batch : meshes.batches) {
        const engine::TextureHandle textureHandle = TextureForId(batch.textureId);
        const Texture2D* texture = assets.GetTexture(textureHandle);
        material.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                ? *texture
                : defaultMaterialTexture;

        const Texture2D* decalTexture = nullptr;
        if (!batch.decalTextureId.empty()) {
            decalTexture = assets.GetTexture(TextureForId(batch.decalTextureId));
        }

        const int hasDecal = decalTexture != nullptr ? 1 : 0;
        const int hasLightmap = batch.receivesLightmap ? 1 : 0;
        const int alphaTest = batch.alphaTest ? 1 : 0;
        const float alphaCutoff = batch.alphaCutoff;
        const float decalOpacity = batch.decalOpacity;
        const int decalEmissive = hasDecal != 0 && batch.decalEmissive ? 1 : 0;
        const Vector3 decalTint = hasDecal != 0 ? batch.decalTint : Vector3{1.0f, 1.0f, 1.0f};
        material.maps[MATERIAL_MAP_NORMAL].texture = (decalTexture != nullptr)
                ? *decalTexture
                : Texture2D{};
        if (hasLightmapLoc >= 0) {
            SetShaderValue(material.shader, hasLightmapLoc, &hasLightmap, SHADER_UNIFORM_INT);
        }
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
        if (decalTintLoc >= 0) {
            SetShaderValue(material.shader, decalTintLoc, &decalTint, SHADER_UNIFORM_VEC3);
        }
        DrawMesh(batch.mesh, material, MatrixIdentity());
    }
    EndMode3D();
}

bool SectorMeshPreview::EnsureBloomResources(int sceneWidth, int sceneHeight)
{
    if (sceneWidth <= 0 || sceneHeight <= 0) {
        return false;
    }

    const int bloomWidth = std::max(1, static_cast<int>(std::round(static_cast<float>(sceneWidth) / static_cast<float>(BloomDownsample))));
    const int bloomHeight = std::max(1, static_cast<int>(std::round(static_cast<float>(sceneHeight) / static_cast<float>(BloomDownsample))));
    const bool dimensionsChanged = bloomSceneWidth != sceneWidth
            || bloomSceneHeight != sceneHeight
            || bloomTargetWidth != bloomWidth
            || bloomTargetHeight != bloomHeight;
    if (dimensionsChanged) {
        UnloadBloomResources();
    }

    if (!bloomSourceMaterialLoaded) {
        if (!LoadBloomSourceMaterial(
                    bloomSourceMaterial,
                    bloomDefaultMaterialTexture,
                    bloomSourceMaterialLoaded,
                    bloomHasDecalLoc,
                    bloomDecalOpacityLoc,
                    bloomDecalEmissiveLoc,
                    bloomDecalTintLoc,
                    bloomDecalIntensityLoc)) {
            return false;
        }
    }

    if (!IsShaderValid(blurShader)) {
        blurShader = LoadShaderFromMemory(nullptr, BloomBlurFs);
        if (!IsShaderValid(blurShader)) {
            return false;
        }
        blurTexelSizeLoc = GetShaderLocation(blurShader, "texelSize");
        blurDirectionLoc = GetShaderLocation(blurShader, "direction");
    }

    if (!IsShaderValid(compositeShader)) {
        compositeShader = LoadShaderFromMemory(nullptr, BloomCompositeFs);
        if (!IsShaderValid(compositeShader)) {
            return false;
        }
        compositeStrengthLoc = GetShaderLocation(compositeShader, "bloomStrength");
        compositeBloomTextureLoc = GetShaderLocation(compositeShader, "bloomTexture");
    }

    if (bloomSceneCopy.texture.id == 0) {
        bloomSceneCopy = LoadRenderTexture(sceneWidth, sceneHeight);
        SetTextureFilter(bloomSceneCopy.texture, TEXTURE_FILTER_BILINEAR);
        bloomSource = LoadRenderTexture(bloomWidth, bloomHeight);
        bloomBlurA = LoadRenderTexture(bloomWidth, bloomHeight);
        bloomBlurB = LoadRenderTexture(bloomWidth, bloomHeight);
        SetTextureFilter(bloomSource.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(bloomBlurA.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(bloomBlurB.texture, TEXTURE_FILTER_BILINEAR);
        bloomSceneWidth = sceneWidth;
        bloomSceneHeight = sceneHeight;
        bloomTargetWidth = bloomWidth;
        bloomTargetHeight = bloomHeight;
    }

    return bloomSceneCopy.texture.id != 0
            && bloomSource.texture.id != 0
            && bloomBlurA.texture.id != 0
            && bloomBlurB.texture.id != 0;
}

void SectorMeshPreview::UnloadBloomResources()
{
    if (bloomSourceMaterialLoaded) {
        bloomSourceMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = bloomDefaultMaterialTexture;
        bloomSourceMaterial.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
        bloomSourceMaterial.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
        UnloadMaterial(bloomSourceMaterial);
        bloomSourceMaterial = Material{};
        bloomDefaultMaterialTexture = Texture2D{};
        bloomSourceMaterialLoaded = false;
    }
    if (IsShaderValid(blurShader)) {
        UnloadShader(blurShader);
        blurShader = Shader{};
    }
    if (IsShaderValid(compositeShader)) {
        UnloadShader(compositeShader);
        compositeShader = Shader{};
    }
    blurTexelSizeLoc = -1;
    blurDirectionLoc = -1;
    compositeStrengthLoc = -1;
    compositeBloomTextureLoc = -1;
    if (bloomSceneCopy.texture.id != 0) {
        UnloadRenderTexture(bloomSceneCopy);
        bloomSceneCopy = RenderTexture2D{};
    }
    if (bloomSource.texture.id != 0) {
        UnloadRenderTexture(bloomSource);
        bloomSource = RenderTexture2D{};
    }
    if (bloomBlurA.texture.id != 0) {
        UnloadRenderTexture(bloomBlurA);
        bloomBlurA = RenderTexture2D{};
    }
    if (bloomBlurB.texture.id != 0) {
        UnloadRenderTexture(bloomBlurB);
        bloomBlurB = RenderTexture2D{};
    }
    bloomSceneWidth = 0;
    bloomSceneHeight = 0;
    bloomTargetWidth = 0;
    bloomTargetHeight = 0;
}

void SectorMeshPreview::RenderBloomSource(engine::AssetManager& assets)
{
    BeginMode3D(camera);
    bloomSourceMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = bloomDefaultMaterialTexture;
    bloomSourceMaterial.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};

    for (const SectorMeshBatch& batch : meshes.batches) {
        const Texture2D* decalTexture = nullptr;
        if (!batch.decalTextureId.empty()) {
            decalTexture = assets.GetTexture(TextureForId(batch.decalTextureId));
        }
        if (decalTexture == nullptr || !batch.decalEmissive) {
            continue;
        }

        const int hasDecal = 1;
        const float decalOpacity = batch.decalOpacity;
        const int decalEmissive = 1;
        const Vector3 decalTint = batch.decalTint;
        const float decalBloomIntensity = batch.decalBloomIntensity;
        bloomSourceMaterial.maps[MATERIAL_MAP_NORMAL].texture = *decalTexture;
        if (bloomHasDecalLoc >= 0) {
            SetShaderValue(bloomSourceMaterial.shader, bloomHasDecalLoc, &hasDecal, SHADER_UNIFORM_INT);
        }
        if (bloomDecalOpacityLoc >= 0) {
            SetShaderValue(bloomSourceMaterial.shader, bloomDecalOpacityLoc, &decalOpacity, SHADER_UNIFORM_FLOAT);
        }
        if (bloomDecalEmissiveLoc >= 0) {
            SetShaderValue(bloomSourceMaterial.shader, bloomDecalEmissiveLoc, &decalEmissive, SHADER_UNIFORM_INT);
        }
        if (bloomDecalTintLoc >= 0) {
            SetShaderValue(bloomSourceMaterial.shader, bloomDecalTintLoc, &decalTint, SHADER_UNIFORM_VEC3);
        }
        if (bloomDecalIntensityLoc >= 0) {
            SetShaderValue(bloomSourceMaterial.shader, bloomDecalIntensityLoc, &decalBloomIntensity, SHADER_UNIFORM_FLOAT);
        }
        DrawMesh(batch.mesh, bloomSourceMaterial, MatrixIdentity());
    }
    EndMode3D();
}

void SectorMeshPreview::UnloadSkyCylinderMesh()
{
    if (skyCylinderMesh.vertexCount > 0) {
        UnloadMesh(skyCylinderMesh);
        skyCylinderMesh = Mesh{};
    }
    if (skyTopCapMesh.vertexCount > 0) {
        UnloadMesh(skyTopCapMesh);
        skyTopCapMesh = Mesh{};
    }

    if (skyMaterialLoaded) {
        skyMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = skyDefaultMaterialTexture;
        UnloadMaterial(skyMaterial);
        skyMaterial = Material{};
        skyDefaultMaterialTexture = Texture2D{};
        skyMaterialLoaded = false;
    }
}

void SectorMeshPreview::DrawSkyCylinder(const Texture2D& texture)
{
    const Matrix transform = MatrixMultiply(
            MatrixRotateY(skyYawOffsetDegrees * DEG2RAD),
            MatrixTranslate(camera.position.x, camera.position.y, camera.position.z));
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    rlDisableDepthMask();
    DrawMesh(skyCylinderMesh, skyMaterial, transform);
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = skyDefaultMaterialTexture;
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].color = skyTopCapColor;
    DrawMesh(skyTopCapMesh, skyMaterial, transform);
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    rlEnableDepthMask();
}

void SectorMeshPreview::ApplyEmissiveDecalBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget)
{
    ApplyEmissiveDecalBloomToScene(assets, sceneTarget);
}

void SectorMeshPreview::ApplyEmissiveDecalBloomToScene(engine::AssetManager& assets, RenderTexture2D& sceneTarget)
{
    if (!initialized || !BloomEnabled || sceneTarget.texture.id == 0) {
        return;
    }
    if (!EnsureBloomResources(sceneTarget.texture.width, sceneTarget.texture.height)) {
        return;
    }

    BeginTextureMode(bloomSceneCopy);
    ClearBackground(BLANK);
    DrawTexturePro(
            sceneTarget.texture,
            FullTextureSrcRect(sceneTarget.texture),
            FullTextureDstRect(bloomSceneCopy.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);
    EndTextureMode();

    BeginTextureMode(bloomSource);
    ClearBackground(BLANK);
    RenderBloomSource(assets);
    EndTextureMode();

    RenderTexture2D* input = &bloomSource;
    RenderTexture2D* output = &bloomBlurA;
    for (int i = 0; i < BloomIterations; ++i) {
        const Vector2 texelSize{
                1.0f / static_cast<float>(input->texture.width),
                1.0f / static_cast<float>(input->texture.height)
        };
        Vector2 direction{1.0f, 0.0f};
        BeginTextureMode(*output);
        ClearBackground(BLANK);
        SetShaderValue(blurShader, blurTexelSizeLoc, &texelSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(blurShader, blurDirectionLoc, &direction, SHADER_UNIFORM_VEC2);
        BeginShaderMode(blurShader);
        DrawTexturePro(
                input->texture,
                FullTextureSrcRect(input->texture),
                FullTextureDstRect(output->texture),
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE);
        EndShaderMode();
        EndTextureMode();

        input = output;
        output = (output == &bloomBlurA) ? &bloomBlurB : &bloomBlurA;
        direction = Vector2{0.0f, 1.0f};
        BeginTextureMode(*output);
        ClearBackground(BLANK);
        SetShaderValue(blurShader, blurTexelSizeLoc, &texelSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(blurShader, blurDirectionLoc, &direction, SHADER_UNIFORM_VEC2);
        BeginShaderMode(blurShader);
        DrawTexturePro(
                input->texture,
                FullTextureSrcRect(input->texture),
                FullTextureDstRect(output->texture),
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE);
        EndShaderMode();
        EndTextureMode();

        input = output;
        output = (output == &bloomBlurA) ? &bloomBlurB : &bloomBlurA;
    }

    BeginTextureMode(sceneTarget);
    ClearBackground(BLANK);
    const float compositeStrength = BloomStrength * BloomLdrIntensityScale;
    SetShaderValue(compositeShader, compositeStrengthLoc, &compositeStrength, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(compositeShader);
    SetShaderValueTexture(compositeShader, compositeBloomTextureLoc, input->texture);
    DrawTexturePro(
            bloomSceneCopy.texture,
            FullTextureSrcRect(bloomSceneCopy.texture),
            FullTextureDstRect(sceneTarget.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);
    EndShaderMode();
    EndTextureMode();
}

SectorViewPose SectorMeshPreview::Pose() const
{
    return RendererPose();
}

SectorViewPose SectorMeshPreview::RendererPose() const
{
    return SectorViewPose{position, yawRadians, pitchRadians};
}

void SectorMeshPreview::ApplyPose(const SectorViewPose& pose)
{
    ApplyRendererPose(pose);
}

void SectorMeshPreview::ApplyRendererPose(const SectorViewPose& pose)
{
    position = pose.position;
    yawRadians = pose.yawRadians;
    pitchRadians = pose.pitchRadians;
    UpdateCamera();
}

float SectorMeshPreview::AssetProgress(engine::AssetManager& assets) const
{
    return RendererAssetProgress(assets);
}

float SectorMeshPreview::RendererAssetProgress(engine::AssetManager& assets) const
{
    return engine::IsNull(assetScope) ? 1.0f : assets.GetScopeProgress(assetScope);
}

const char* SectorMeshPreview::LightmapStatusText() const
{
    return RendererLightmapStatusText();
}

const char* SectorMeshPreview::RendererLightmapStatusText() const
{
    return SectorLightmapStatusText(static_cast<SectorLightmapStatus>(lightmapStatus));
}

std::string SectorMeshPreview::ResolveAssetPath(const std::string& path)
{
    if (StartsWith(path, "assets/")) {
        return std::string(ASSETS_PATH) + path.substr(7);
    }

    return path;
}

engine::TextureHandle SectorMeshPreview::TextureForId(const std::string& textureId) const
{
    const auto it = textureHandlesById.find(textureId);
    if (it == textureHandlesById.end()) {
        return engine::NullTextureHandle();
    }

    return it->second;
}

void SectorMeshPreview::UpdateCamera()
{
    const float cosPitch = std::cos(pitchRadians);
    const Vector3 look{
            std::cos(yawRadians) * cosPitch,
            std::sin(pitchRadians),
            std::sin(yawRadians) * cosPitch
    };

    camera.position = position;
    camera.target = Vector3Add(position, look);
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
}

} // namespace game
