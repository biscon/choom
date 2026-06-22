#include "sector_demo/SectorMeshPreview.h"

#include "engine/assets/TextureLoadFlags.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMeshBuilder.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace game {

namespace {

constexpr float MouseSensitivity = 0.0030f;
constexpr float MoveSpeed = 5.0f;
constexpr float PitchLimit = 1.45f;
constexpr int MouseLookWarmupFrames = 2;

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
uniform int hasDecal;
uniform float decalOpacity;
uniform int decalEmissive;
uniform vec3 decalTint;

out vec4 finalColor;

void main()
{
    vec4 baseColor = texture(texture0, fragTexCoord);
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
    vec4 bakedSample = (useLightmap > 0.5) ? texture(texture1, fragTexCoord2) : vec4(0.0, 0.0, 0.0, 1.0);
    float aoFactor = (useBakedAmbientOcclusion > 0.5) ? bakedSample.a : 1.0;
    vec3 ambient = fragColor.rgb * aoFactor;
    vec3 bakedDirect = bakedSample.rgb;
    vec3 lighting = clamp(ambient + bakedDirect, 0.0, 1.0);
    vec3 litRgb = surfaceRgb * lighting;
    finalColor = vec4(mix(litRgb, emissiveDecalRgb, emissiveDecalAlpha), baseColor.a * fragColor.a);
}
)";

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
    hasDecalLoc = GetShaderLocation(material.shader, "hasDecal");
    decalOpacityLoc = GetShaderLocation(material.shader, "decalOpacity");
    decalEmissiveLoc = GetShaderLocation(material.shader, "decalEmissive");
    decalTintLoc = GetShaderLocation(material.shader, "decalTint");
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

} // namespace

bool SectorMeshPreview::Rebuild(
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
    mouseLookEnabled = true;
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateCamera();

    initialized = true;
    Enter();
    return true;
}

void SectorMeshPreview::Shutdown(engine::AssetManager& assets)
{
    generatedGeometry = {};
    if (!initialized
            && engine::IsNull(assetScope)
            && meshes.batches.empty()
            && !materialLoaded) {
        return;
    }

    Leave();
    UnloadSectorMeshes(meshes);
    textureHandlesById.clear();
    lightmapTexture = engine::NullTextureHandle();
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

void SectorMeshPreview::Enter()
{
    if (!initialized) {
        return;
    }

    mouseLookEnabled = true;
    mouseLookWarmupFrames = MouseLookWarmupFrames;
    DisableCursor();
}

void SectorMeshPreview::Leave()
{
    EnableCursor();
}

void SectorMeshPreview::Update(engine::Input& input, float dt)
{
    if (!initialized) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key != KEY_F11) {
                    return;
                }

                mouseLookEnabled = !mouseLookEnabled;
                if (mouseLookEnabled) {
                    mouseLookWarmupFrames = MouseLookWarmupFrames;
                    DisableCursor();
                } else {
                    EnableCursor();
                }
                engine::ConsumeEvent(event);
            }
    );

    if (mouseLookEnabled) {
        if (mouseLookWarmupFrames > 0) {
            --mouseLookWarmupFrames;
        } else {
            const Vector2 mouseDelta = input.MouseDelta();
            yawRadians += mouseDelta.x * MouseSensitivity;
            pitchRadians -= mouseDelta.y * MouseSensitivity;
            pitchRadians = Clamp(pitchRadians, -PitchLimit, PitchLimit);
        }
    }

    if (mouseLookEnabled) {
        Vector3 forward{std::cos(yawRadians), 0.0f, std::sin(yawRadians)};
        Vector3 right{-forward.z, 0.0f, forward.x};
        Vector3 movement{};

        if (input.IsKeyDown(KEY_W)) {
            movement = Vector3Add(movement, forward);
        }
        if (input.IsKeyDown(KEY_S)) {
            movement = Vector3Subtract(movement, forward);
        }
        if (input.IsKeyDown(KEY_D)) {
            movement = Vector3Add(movement, right);
        }
        if (input.IsKeyDown(KEY_A)) {
            movement = Vector3Subtract(movement, right);
        }
        if (input.IsKeyDown(KEY_SPACE)) {
            movement.y += 1.0f;
        }
        if (input.IsKeyDown(KEY_LEFT_CONTROL) || input.IsKeyDown(KEY_RIGHT_CONTROL)) {
            movement.y -= 1.0f;
        }

        if (Vector3LengthSqr(movement) > 0.0001f) {
            movement = Vector3Normalize(movement);
            position = Vector3Add(position, Vector3Scale(movement, MoveSpeed * dt));
        }
    }

    UpdateCamera();
}

void SectorMeshPreview::Render(engine::AssetManager& assets, bool useBakedAmbientOcclusion)
{
    if (!initialized) {
        return;
    }

    BeginMode3D(camera);
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
        const float decalOpacity = batch.decalOpacity;
        const int decalEmissive = hasDecal != 0 && batch.decalEmissive ? 1 : 0;
        const Vector3 decalTint = hasDecal != 0 ? batch.decalTint : Vector3{1.0f, 1.0f, 1.0f};
        material.maps[MATERIAL_MAP_NORMAL].texture = (decalTexture != nullptr)
                ? *decalTexture
                : Texture2D{};
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

SectorMeshPreviewPose SectorMeshPreview::Pose() const
{
    return SectorMeshPreviewPose{position, yawRadians, pitchRadians};
}

void SectorMeshPreview::ApplyPose(const SectorMeshPreviewPose& pose)
{
    position = pose.position;
    yawRadians = pose.yawRadians;
    pitchRadians = pose.pitchRadians;
    UpdateCamera();
}

void SectorMeshPreview::SetMouseLookEnabled(bool enabled)
{
    if (!initialized) {
        return;
    }

    mouseLookEnabled = enabled;
    if (mouseLookEnabled) {
        mouseLookWarmupFrames = MouseLookWarmupFrames;
        DisableCursor();
    } else {
        EnableCursor();
    }
}

float SectorMeshPreview::AssetProgress(engine::AssetManager& assets) const
{
    return engine::IsNull(assetScope) ? 1.0f : assets.GetScopeProgress(assetScope);
}

const char* SectorMeshPreview::LightmapStatusText() const
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
