#include "sector_demo/renderer/SectorBloomRenderer.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorMeshBuilder.h"
#include "sector_demo/SectorPortalVisibility.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr bool BloomEnabled = true;
constexpr float BloomStrength = 0.5f;
constexpr float BloomLdrIntensityScale = 10.0f;
constexpr int BloomIterations = 3;
constexpr int BloomDownsample = 4;

const char* SectorBloomSourceVs = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec4 vertexTangent;
in vec4 vertexColor;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec2 fragTexCoord2;
out vec2 fragDecalUv;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragTexCoord2 = vertexTexCoord2;
    fragDecalUv = vertexTangent.xy;
    fragWorldPosition = vertexPosition;
    fragWorldNormal = normalize(vertexNormal);
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* SectorBloomSourceFs = R"(
#version 330
in vec2 fragTexCoord;
in vec2 fragTexCoord2;
in vec2 fragDecalUv;
in vec3 fragWorldPosition;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D decalTexture;
uniform int hasDecal;
uniform float decalOpacity;
uniform int decalEmissive;
uniform vec3 decalTint;
uniform float decalBloomIntensity;

uniform int fogEnabled;
uniform vec3 fogCameraPosition;
uniform float fogStartDistanceWorld;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeightWorld;
uniform float fogHeightFalloff;

out vec4 finalColor;

float SectorFogTransmittance(vec3 worldPosition)
{
    if (fogEnabled == 0 || fogDensity <= 0.0 || fogMaxOpacity <= 0.0) {
        return 1.0;
    }

    float fogDistance = max(length(worldPosition - fogCameraPosition) - fogStartDistanceWorld, 0.0);
    float midpointHeight = (fogCameraPosition.y + worldPosition.y) * 0.5;
    float heightAboveReference = max(midpointHeight - fogReferenceHeightWorld, 0.0);
    float heightMultiplier = exp(-heightAboveReference * fogHeightFalloff);
    float fogAmount = min(
            1.0 - exp(-fogDensity * fogDistance * heightMultiplier),
            fogMaxOpacity);
    return 1.0 - fogAmount;
}

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
    vec3 rgb = decalColor.rgb * decalTint * alpha * (decalBloomIntensity / 10.0)
            * SectorFogTransmittance(fragWorldPosition);
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

bool LoadBloomSourceMaterial(
        Material& material,
        Texture2D& defaultMaterialTexture,
        bool& materialLoaded,
        int& hasDecalLoc,
        int& decalOpacityLoc,
        int& decalEmissiveLoc,
        int& decalTintLoc,
        int& decalBloomIntensityLoc,
        SectorFogShaderLocations& fogShaderLocations)
{
    material = LoadMaterialDefault();
    Shader shader = LoadShaderFromMemory(SectorBloomSourceVs, SectorBloomSourceFs);
    if (shader.id == 0) {
        UnloadMaterial(material);
        material = Material{};
        return false;
    }
    material.shader = shader;
    material.shader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocationAttrib(material.shader, "vertexNormal");
    material.shader.locs[SHADER_LOC_VERTEX_TANGENT] = GetShaderLocationAttrib(material.shader, "vertexTangent");
    material.shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(material.shader, "texture0");
    material.shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(material.shader, "texture1");
    material.shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(material.shader, "decalTexture");
    hasDecalLoc = GetShaderLocation(material.shader, "hasDecal");
    decalOpacityLoc = GetShaderLocation(material.shader, "decalOpacity");
    decalEmissiveLoc = GetShaderLocation(material.shader, "decalEmissive");
    decalTintLoc = GetShaderLocation(material.shader, "decalTint");
    decalBloomIntensityLoc = GetShaderLocation(material.shader, "decalBloomIntensity");
    fogShaderLocations = GetSectorFogShaderLocations(material.shader);
    defaultMaterialTexture = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    materialLoaded = true;
    return true;
}

} // namespace

void SectorBloomRenderer::Shutdown()
{
    if (sourceMaterialLoaded) {
        sourceMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = defaultMaterialTexture;
        sourceMaterial.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
        sourceMaterial.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
        UnloadMaterial(sourceMaterial);
        sourceMaterial = Material{};
        defaultMaterialTexture = Texture2D{};
        sourceMaterialLoaded = false;
    }
    if (IsShaderValid(blurShader)) {
        UnloadShader(blurShader);
        blurShader = Shader{};
    }
    if (IsShaderValid(compositeShader)) {
        UnloadShader(compositeShader);
        compositeShader = Shader{};
    }
    hasDecalLoc = -1;
    decalOpacityLoc = -1;
    decalEmissiveLoc = -1;
    decalTintLoc = -1;
    decalIntensityLoc = -1;
    fogShaderLocations = SectorFogShaderLocations{};
    blurTexelSizeLoc = -1;
    blurDirectionLoc = -1;
    compositeStrengthLoc = -1;
    compositeBloomTextureLoc = -1;
    if (sceneCopy.texture.id != 0) {
        UnloadRenderTexture(sceneCopy);
        sceneCopy = RenderTexture2D{};
    }
    if (source.texture.id != 0) {
        UnloadRenderTexture(source);
        source = RenderTexture2D{};
    }
    if (blurA.texture.id != 0) {
        UnloadRenderTexture(blurA);
        blurA = RenderTexture2D{};
    }
    if (blurB.texture.id != 0) {
        UnloadRenderTexture(blurB);
        blurB = RenderTexture2D{};
    }
    sceneWidth = 0;
    sceneHeight = 0;
    targetWidth = 0;
    targetHeight = 0;
}

void SectorBloomRenderer::ApplyEmissiveDecalBloomToScene(
        engine::AssetManager& assets,
        bool previewInitialized,
        const Camera3D& camera,
        const std::vector<SectorMeshBatch>& sectorDrawRecords,
        const RuntimePortalVisibilityResult& visibilityResult,
        const std::unordered_map<std::string, engine::TextureHandle>& textureHandlesById,
        RenderTexture2D& sceneTarget,
        const SectorFogRenderContext& fogContext)
{
    if (!previewInitialized || !BloomEnabled || sceneTarget.texture.id == 0) {
        return;
    }
    if (!EnsureResources(sceneTarget.texture.width, sceneTarget.texture.height)) {
        return;
    }

    BeginTextureMode(sceneCopy);
    ClearBackground(BLANK);
    DrawTexturePro(
            sceneTarget.texture,
            FullTextureSrcRect(sceneTarget.texture),
            FullTextureDstRect(sceneCopy.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);
    EndTextureMode();

    BeginTextureMode(source);
    ClearBackground(BLANK);
    RenderBloomSource(
            assets,
            camera,
            sectorDrawRecords,
            visibilityResult,
            textureHandlesById,
            fogContext);
    EndTextureMode();

    RenderTexture2D* input = &source;
    RenderTexture2D* output = &blurA;
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
        output = (output == &blurA) ? &blurB : &blurA;
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
        output = (output == &blurA) ? &blurB : &blurA;
    }

    BeginTextureMode(sceneTarget);
    ClearBackground(BLANK);
    const float compositeStrength = BloomStrength * BloomLdrIntensityScale;
    SetShaderValue(compositeShader, compositeStrengthLoc, &compositeStrength, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(compositeShader);
    SetShaderValueTexture(compositeShader, compositeBloomTextureLoc, input->texture);
    DrawTexturePro(
            sceneCopy.texture,
            FullTextureSrcRect(sceneCopy.texture),
            FullTextureDstRect(sceneTarget.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);
    EndShaderMode();
    EndTextureMode();
}

bool SectorBloomRenderer::IsLoaded() const
{
    return sourceMaterialLoaded
            || IsShaderValid(blurShader)
            || IsShaderValid(compositeShader)
            || sceneCopy.texture.id != 0
            || source.texture.id != 0
            || blurA.texture.id != 0
            || blurB.texture.id != 0;
}

bool SectorBloomRenderer::EnsureResources(int sceneWidthValue, int sceneHeightValue)
{
    if (sceneWidthValue <= 0 || sceneHeightValue <= 0) {
        return false;
    }

    const int bloomWidth = std::max(
            1,
            static_cast<int>(std::round(static_cast<float>(sceneWidthValue) / static_cast<float>(BloomDownsample))));
    const int bloomHeight = std::max(
            1,
            static_cast<int>(std::round(static_cast<float>(sceneHeightValue) / static_cast<float>(BloomDownsample))));
    const bool dimensionsChanged = sceneWidth != sceneWidthValue
            || sceneHeight != sceneHeightValue
            || targetWidth != bloomWidth
            || targetHeight != bloomHeight;
    if (dimensionsChanged) {
        Shutdown();
    }

    if (!sourceMaterialLoaded) {
        if (!LoadBloomSourceMaterial(
                    sourceMaterial,
                    defaultMaterialTexture,
                    sourceMaterialLoaded,
                    hasDecalLoc,
                    decalOpacityLoc,
                    decalEmissiveLoc,
                    decalTintLoc,
                    decalIntensityLoc,
                    fogShaderLocations)) {
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

    if (sceneCopy.texture.id == 0) {
        sceneCopy = LoadRenderTexture(sceneWidthValue, sceneHeightValue);
        SetTextureFilter(sceneCopy.texture, TEXTURE_FILTER_BILINEAR);
        source = LoadRenderTexture(bloomWidth, bloomHeight);
        blurA = LoadRenderTexture(bloomWidth, bloomHeight);
        blurB = LoadRenderTexture(bloomWidth, bloomHeight);
        SetTextureFilter(source.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(blurA.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(blurB.texture, TEXTURE_FILTER_BILINEAR);
        sceneWidth = sceneWidthValue;
        sceneHeight = sceneHeightValue;
        targetWidth = bloomWidth;
        targetHeight = bloomHeight;
    }

    return sceneCopy.texture.id != 0
            && source.texture.id != 0
            && blurA.texture.id != 0
            && blurB.texture.id != 0;
}

void SectorBloomRenderer::RenderBloomSource(
        engine::AssetManager& assets,
        const Camera3D& camera,
        const std::vector<SectorMeshBatch>& sectorDrawRecords,
        const RuntimePortalVisibilityResult& visibilityResult,
        const std::unordered_map<std::string, engine::TextureHandle>& textureHandlesById,
        const SectorFogRenderContext& fogContext)
{
    BeginMode3D(camera);
    UploadSectorFogShaderValues(sourceMaterial.shader, fogShaderLocations, fogContext);
    sourceMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = defaultMaterialTexture;
    sourceMaterial.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};

    for (const SectorMeshBatch& batch : sectorDrawRecords) {
        if (!ShouldDrawEmissiveBloomSectorMeshRecordForVisibility(batch, visibilityResult)) {
            continue;
        }

        const Texture2D* decalTexture = nullptr;
        if (!batch.decalTextureId.empty()) {
            decalTexture = assets.GetTexture(TextureForId(textureHandlesById, batch.decalTextureId));
        }
        if (decalTexture == nullptr) {
            continue;
        }

        const int hasDecal = 1;
        const float decalOpacity = batch.decalOpacity;
        const int decalEmissive = 1;
        const Vector3 decalTint = batch.decalTint;
        const float decalBloomIntensity = batch.decalBloomIntensity;
        sourceMaterial.maps[MATERIAL_MAP_NORMAL].texture = *decalTexture;
        if (hasDecalLoc >= 0) {
            SetShaderValue(sourceMaterial.shader, hasDecalLoc, &hasDecal, SHADER_UNIFORM_INT);
        }
        if (decalOpacityLoc >= 0) {
            SetShaderValue(sourceMaterial.shader, decalOpacityLoc, &decalOpacity, SHADER_UNIFORM_FLOAT);
        }
        if (decalEmissiveLoc >= 0) {
            SetShaderValue(sourceMaterial.shader, decalEmissiveLoc, &decalEmissive, SHADER_UNIFORM_INT);
        }
        if (decalTintLoc >= 0) {
            SetShaderValue(sourceMaterial.shader, decalTintLoc, &decalTint, SHADER_UNIFORM_VEC3);
        }
        if (decalIntensityLoc >= 0) {
            SetShaderValue(sourceMaterial.shader, decalIntensityLoc, &decalBloomIntensity, SHADER_UNIFORM_FLOAT);
        }
        DrawMesh(batch.mesh, sourceMaterial, MatrixIdentity());
    }
    EndMode3D();
}

engine::TextureHandle SectorBloomRenderer::TextureForId(
        const std::unordered_map<std::string, engine::TextureHandle>& textureHandlesById,
        const std::string& textureId) const
{
    const auto it = textureHandlesById.find(textureId);
    if (it == textureHandlesById.end()) {
        return engine::NullTextureHandle();
    }

    return it->second;
}

} // namespace game
