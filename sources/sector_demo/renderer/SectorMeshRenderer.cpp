#include "sector_demo/renderer/SectorMeshRenderer.h"

#include "sector_demo/renderer/SectorDynamicShadowSampling.h"
#include "sector_demo/renderer/SectorFlashlightProfileSampling.h"

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

const char* HdrCompositeVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragUv;
uniform mat4 mvp;
void main() { fragUv=vertexTexCoord; gl_Position=mvp*vec4(vertexPosition,1.0); }
)";

const char* HdrCompositeFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D sceneColor;
uniform sampler2D sourceColor;
uniform int compositeMode;
float safeRadianceChannel(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? 65504.0 : 0.0;
    return min(max(value,0.0),65504.0);
}
vec3 safeRadiance(vec3 value) {
    return vec3(safeRadianceChannel(value.r),safeRadianceChannel(value.g),
            safeRadianceChannel(value.b));
}
float safeAlpha(float value) {
    return (isnan(value)||isinf(value))?1.0:clamp(value,0.0,1.0);
}
void main() {
    vec4 source=texture(sourceColor,fragUv);
    if (compositeMode == 1) {
        vec4 scene=texture(sceneColor,fragUv);
        float coverage=isnan(source.a)||isinf(source.a)?0.0:clamp(source.a,0.0,1.0);
        finalColor=vec4(safeRadiance(safeRadiance(scene.rgb)*(1.0-coverage)
                +safeRadiance(source.rgb)),safeAlpha(scene.a));
        return;
    }
    finalColor=vec4(clamp(safeRadiance(source.rgb),vec3(0.0),vec3(65504.0)),safeAlpha(source.a));
}
)";

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

const char* SectorLightmapVs = R"(
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

const char* SectorLightmapFs = R"(
#version 330
in vec2 fragTexCoord;
in vec2 fragTexCoord2;
in vec2 fragDecalUv;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D decalTexture;
uniform sampler2D normalTexture;
uniform sampler2D directionalLightmapTexture;
uniform samplerCube environmentTexture;
uniform float useLightmap;
uniform float useBakedAmbientOcclusion;
uniform int hasLightmap;
uniform int hasDirectionalLightmap;
uniform int hasNormalMap;
uniform float normalStrength;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3 cameraPosition;
uniform int hasEnvironment;
uniform float environmentExposure;
uniform float indirectDiffuseScale;
uniform float environmentSpecularScale;
uniform int environmentBoxProjection;
uniform vec3 environmentCapturePosition;
uniform vec3 environmentInfluenceCenter;
uniform vec3 environmentHalfExtents;
uniform float environmentYaw;
uniform float environmentMaxLod;
uniform float environmentIntensity;
uniform int pbrDiagnosticMode;
uniform int alphaTest;
uniform float alphaCutoff;
uniform int hasDecal;
uniform float decalOpacity;
uniform int decalEmissive;
uniform float decalEmissiveStrength;
uniform vec3 decalTint;

uniform int fogEnabled;
uniform vec3 fogColor;
uniform vec3 fogCameraPosition;
uniform float fogStartDistanceWorld;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeightWorld;
uniform float fogHeightFalloff;

#define MAX_DYNAMIC_LIGHTS 32
#define MAX_DYNAMIC_SHADOW_CASTERS 64
uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightTypes[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightDirections[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightSpotShadowRight[MAX_DYNAMIC_LIGHTS];
uniform vec2 dynamicLightSpotShadowProjection[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightProfiles[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightProfileParameters[MAX_DYNAMIC_LIGHTS];
uniform sampler2D flashlightCookie;
uniform int hasPointShadows;
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform int shadowAtlasTilesPerRow;
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

#define MAX_STATIC_SPECULAR_LIGHTS 4
uniform int useStaticSpecularLighting;
uniform int staticSpecularLightCount;
uniform vec3 staticSpecularLightPositions[MAX_STATIC_SPECULAR_LIGHTS];
uniform vec3 staticSpecularLightColors[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightRadii[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightIntensities[MAX_STATIC_SPECULAR_LIGHTS];
uniform int staticSpecularLightTypes[MAX_STATIC_SPECULAR_LIGHTS];
uniform vec3 staticSpecularLightDirections[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightInnerConeCos[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightOuterConeCos[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightStartFeathers[MAX_STATIC_SPECULAR_LIGHTS];

out vec4 finalColor;

const vec2 kPoissonDisk[12] = vec2[12](
    vec2(-0.326, -0.406),
    vec2(-0.840, -0.074),
    vec2(-0.696,  0.457),
    vec2(-0.203,  0.621),
    vec2( 0.962, -0.195),
    vec2( 0.473, -0.480),
    vec2( 0.519,  0.767),
    vec2( 0.185, -0.893),
    vec2( 0.507,  0.064),
    vec2( 0.896,  0.412),
    vec2(-0.322, -0.933),
    vec2(-0.792, -0.598)
);

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSq = dot(value, value);
    return lengthSq > 0.00000001 ? value * inversesqrt(lengthSq) : fallback;
}

)"
SECTOR_FLASHLIGHT_PROFILE_GLSL
R"(
vec3 StoreFiniteHalfRadiance(vec3 value)
{
    vec3 result;
    result.r = isnan(value.r) ? 0.0 : (isinf(value.r) ? (value.r > 0.0 ? 65504.0 : 0.0) : min(max(value.r, 0.0), 65504.0));
    result.g = isnan(value.g) ? 0.0 : (isinf(value.g) ? (value.g > 0.0 ? 65504.0 : 0.0) : min(max(value.g, 0.0), 65504.0));
    result.b = isnan(value.b) ? 0.0 : (isinf(value.b) ? (value.b > 0.0 ? 65504.0 : 0.0) : min(max(value.b, 0.0), 65504.0));
    return result;
}

float DistributionGgx(vec3 normal, vec3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(normal, halfway), 0.0);
    float denominator = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denominator * denominator, 0.000001);
}

float GeometrySchlickGgx(float ndotv, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return ndotv / max(ndotv * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(
        vec3 normal,
        vec3 viewDirection,
        vec3 lightDirection,
        float roughness)
{
    return GeometrySchlickGgx(
            max(dot(normal, viewDirection), 0.0), roughness)
            * GeometrySchlickGgx(
                    max(dot(normal, lightDirection), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0)
            * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec2 EnvironmentBrdfApprox(float roughness, float ndotv)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

)"
SECTOR_DYNAMIC_SURFACE_SHADOW_GLSL
R"(
vec3 SurfaceNormal(vec3 geometricNormal, vec3 tangentNormalSample)
{
    if (hasNormalMap == 0) {
        return geometricNormal;
    }

    vec3 positionDx = dFdx(fragWorldPosition);
    vec3 positionDy = dFdy(fragWorldPosition);
    vec2 uvDx = dFdx(fragTexCoord);
    vec2 uvDy = dFdy(fragTexCoord);
    float uvDeterminant = uvDx.x * uvDy.y - uvDx.y * uvDy.x;
    float uvDerivativeScaleSq = dot(uvDx, uvDx) * dot(uvDy, uvDy);
    if (uvDeterminant * uvDeterminant
                    <= uvDerivativeScaleSq * 0.00000001) {
        return geometricNormal;
    }

    float inverseUvDeterminant = 1.0 / uvDeterminant;
    vec3 tangent = (positionDx * uvDy.y - positionDy * uvDx.y)
            * inverseUvDeterminant;
    vec3 sourceBitangent = (positionDy * uvDx.x - positionDx * uvDy.x)
            * inverseUvDeterminant;
    tangent -= geometricNormal * dot(tangent, geometricNormal);
    if (any(isnan(tangent)) || any(isinf(tangent))
            || any(isnan(sourceBitangent)) || any(isinf(sourceBitangent))
            || dot(tangent, tangent) <= 0.000000000001
            || dot(sourceBitangent, sourceBitangent) <= 0.000000000001) {
        return geometricNormal;
    }

    tangent = normalize(tangent);
    float handedness = dot(cross(geometricNormal, tangent), sourceBitangent) < 0.0
            ? -1.0
            : 1.0;
    vec3 bitangent = SafeNormalize(
            cross(geometricNormal, tangent),
            vec3(0.0, 0.0, 1.0)) * handedness;
    vec3 mappedNormal = tangentNormalSample * 2.0 - 1.0;
    mappedNormal.xy *= normalStrength;
    return SafeNormalize(
            mat3(tangent, bitangent, geometricNormal) * mappedNormal,
            geometricNormal);
}

vec3 ApplyDirectionalLightmap(
        vec3 bakedLighting,
        vec3 geometricNormal,
        vec3 worldNormal)
{
    if (hasDirectionalLightmap == 0 || hasNormalMap == 0) {
        return bakedLighting;
    }
    vec4 directionalSample = texture(
            directionalLightmapTexture, fragTexCoord2);
    float directionalFraction = clamp(directionalSample.a, 0.0, 1.0);
    if (directionalFraction <= 0.0001) {
        return bakedLighting;
    }
    vec3 dominantDirection = SafeNormalize(
            directionalSample.rgb * 2.0 - 1.0,
            geometricNormal);
    float geometricResponse = max(
            dot(geometricNormal, dominantDirection), 0.0);
    if (geometricResponse <= 0.0001) {
        return bakedLighting;
    }
    float mappedResponse = max(dot(worldNormal, dominantDirection), 0.0);
    float responseRatio = clamp(
            mappedResponse / geometricResponse, 0.0, 4.0);
    return bakedLighting * mix(
            1.0, responseRatio, directionalFraction);
}

vec3 ApplySectorFog(
        vec3 surfaceRgb,
        vec3 staticAtmosphericLighting,
        vec3 worldPosition)
{
    if (fogEnabled == 0 || fogDensity <= 0.0 || fogMaxOpacity <= 0.0) {
        return surfaceRgb;
    }

    float fogDistance = max(length(worldPosition - fogCameraPosition) - fogStartDistanceWorld, 0.0);
    float midpointHeight = (fogCameraPosition.y + worldPosition.y) * 0.5;
    float heightAboveReference = max(midpointHeight - fogReferenceHeightWorld, 0.0);
    float heightMultiplier = exp(-heightAboveReference * fogHeightFalloff);
    float fogAmount = min(
            1.0 - exp(-fogDensity * fogDistance * heightMultiplier),
            fogMaxOpacity);
    vec3 fogLighting = max(staticAtmosphericLighting, vec3(0.0));
    float fogLightPeak = max(max(fogLighting.r, fogLighting.g), fogLighting.b);
    float fogLightVisibility = smoothstep(0.0, 0.04, fogLightPeak);
    vec3 fogLightTint = fogLightPeak > 0.00001
            ? clamp(fogLighting / fogLightPeak, vec3(0.0), vec3(1.0))
            : vec3(0.0);
    vec3 fogScattering = fogColor * fogLightTint * fogLightVisibility;
    return surfaceRgb * (1.0 - fogAmount) + fogScattering * fogAmount;
}

void main()
{
    vec3 geometricNormal = SafeNormalize(fragWorldNormal, vec3(0.0, 1.0, 0.0));
    vec3 receiverPlaneNormal = geometricNormal;
    if (hasPointShadows != 0) {
        receiverPlaneNormal = SafeNormalize(
                cross(dFdx(fragWorldPosition), dFdy(fragWorldPosition)),
                geometricNormal);
        if (dot(receiverPlaneNormal, geometricNormal) < 0.0) {
            receiverPlaneNormal = -receiverPlaneNormal;
        }
    }
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
    vec4 bakedSample = (useLightmap > 0.5 && hasLightmap != 0)
            ? texture(texture1, fragTexCoord2)
            : vec4(0.0, 0.0, 0.0, 1.0);
    float aoFactor = (useBakedAmbientOcclusion > 0.5 && hasLightmap != 0)
            ? bakedSample.a
            : 1.0;
    vec3 tangentNormalSample = hasNormalMap != 0
            ? texture(normalTexture, fragTexCoord).xyz
            : vec3(0.5, 0.5, 1.0);
    vec3 worldNormal = SurfaceNormal(
            geometricNormal, tangentNormalSample);
    vec3 viewDirection = SafeNormalize(
            cameraPosition - fragWorldPosition, geometricNormal);
    float metallic = clamp(metallicFactor, 0.0, 1.0);
    float roughness = clamp(roughnessFactor, 0.045, 1.0);
    vec3 f0 = mix(vec3(0.04), surfaceRgb, metallic);
    vec3 correctedBakedLighting = ApplyDirectionalLightmap(
            bakedSample.rgb, geometricNormal, worldNormal);
    vec3 staticLighting = max(
            fragColor.rgb * aoFactor + correctedBakedLighting,
            vec3(0.0));
    vec3 staticDiffuse = surfaceRgb
            * (1.0 - metallic)
            * staticLighting
            * indirectDiffuseScale;
    vec3 dynamicDirectDiffuse = vec3(0.0);
    vec3 dynamicDirectSpecular = vec3(0.0);
    for (int i = 0; i < dynamicLightCount && i < MAX_DYNAMIC_LIGHTS; ++i) {
        float radius = dynamicLightRadii[i];
        vec3 toLight = dynamicLightPositions[i] - fragWorldPosition;
        float emitterAtten = 1.0;
        if (dynamicLightTypes[i] == 2) {
            vec3 emitterNormal = SafeNormalize(dynamicLightDirections[i], vec3(0.0, -1.0, 0.0));
            vec3 emitterRight = SafeNormalize(dynamicLightSpotShadowRight[i], vec3(1.0, 0.0, 0.0));
            vec3 emitterUp = SafeNormalize(cross(emitterRight, emitterNormal), vec3(0.0, 0.0, 1.0));
            vec3 relative = fragWorldPosition - dynamicLightPositions[i];
            vec3 nearest = dynamicLightPositions[i]
                    + emitterRight * clamp(dot(relative, emitterRight), -dynamicLightInnerConeCos[i], dynamicLightInnerConeCos[i])
                    + emitterUp * clamp(dot(relative, emitterUp), -dynamicLightOuterConeCos[i], dynamicLightOuterConeCos[i]);
            toLight = nearest - fragWorldPosition;
            emitterAtten = max(dot(emitterNormal, SafeNormalize(fragWorldPosition - nearest, emitterNormal)), 0.0);
        }
        float distanceSq = dot(toLight, toLight);
        if (radius > 0.0 && distanceSq < radius * radius) {
            float distanceToLight = sqrt(max(distanceSq, 0.0));
            vec3 lightDirection = distanceToLight > 0.0001 ? toLight / distanceToLight : worldNormal;
            float ndotl = max(dot(worldNormal, lightDirection), 0.0);
            float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
            atten *= atten;
            if (ndotl <= 0.0 || atten <= 0.0
                    || dynamicLightIntensities[i] <= 0.0) continue;
            float coneAtten = emitterAtten;
            if (dynamicLightTypes[i] == 1) {
                vec3 spotDirection = SafeNormalize(dynamicLightDirections[i], vec3(0.0, -1.0, 0.0));
                vec3 fragmentDirectionFromLight = distanceToLight > 0.0001
                        ? -lightDirection
                        : spotDirection;
                float coneDot = dot(spotDirection, fragmentDirectionFromLight);
                float innerConeCos = dynamicLightInnerConeCos[i];
                float outerConeCos = dynamicLightOuterConeCos[i];
                if (dynamicLightProfiles[i] == 1) {
                    coneAtten = FlashlightProfileFactor(
                            i, fragmentDirectionFromLight);
                } else {
                    coneAtten = abs(innerConeCos - outerConeCos) > 0.0001
                            ? smoothstep(outerConeCos, innerConeCos, coneDot)
                            : step(innerConeCos, coneDot);
                }
            }
            if (coneAtten <= 0.0) continue;
            int shadowSlot = dynamicLightShadowSlots[i];
            if (shadowSlot >= 0 && shadowStrength[shadowSlot] > 0.0) {
                float visibility = DynamicLightShadowVisibility(
                        i, shadowSlot, fragWorldPosition, receiverPlaneNormal, lightDirection);
                coneAtten *= mix(1.0, visibility,
                        clamp(shadowStrength[shadowSlot], 0.0, 1.0));
            }
            vec3 halfway = SafeNormalize(
                    viewDirection + lightDirection, worldNormal);
            float distribution = DistributionGgx(
                    worldNormal, halfway, roughness);
            float geometry = GeometrySmith(
                    worldNormal, viewDirection, lightDirection, roughness);
            vec3 fresnel = FresnelSchlick(
                    max(dot(halfway, viewDirection), 0.0), f0);
            vec3 specular = distribution * geometry * fresnel
                    / max(4.0
                            * max(dot(worldNormal, viewDirection), 0.0)
                            * ndotl,
                            0.001);
            vec3 diffuseWeight = (vec3(1.0) - fresnel)
                    * (1.0 - metallic);
            vec3 radiance = dynamicLightColors[i]
                    * dynamicLightIntensities[i]
                    * atten
                    * coneAtten;
            dynamicDirectDiffuse += diffuseWeight
                    * surfaceRgb
                    * radiance
                    * ndotl;
            dynamicDirectSpecular += specular * radiance * ndotl;
        }
    }

    vec3 staticDirectSpecular = vec3(0.0);
    if (useStaticSpecularLighting != 0) {
        for (int i = 0;
                i < staticSpecularLightCount
                        && i < MAX_STATIC_SPECULAR_LIGHTS;
                ++i) {
            float radius = staticSpecularLightRadii[i];
            vec3 toLight = staticSpecularLightPositions[i]
                    - fragWorldPosition;
            float distanceSq = dot(toLight, toLight);
            if (radius <= 0.0 || distanceSq >= radius * radius) continue;
            float distanceToLight = sqrt(max(distanceSq, 0.0));
            vec3 lightDirection = distanceToLight > 0.0001
                    ? toLight / distanceToLight
                    : worldNormal;
            float ndotl = max(dot(worldNormal, lightDirection), 0.0);
            if (ndotl <= 0.0) continue;
            float atten = clamp(
                    1.0 - distanceToLight / radius, 0.0, 1.0);
            atten *= atten;
            float coneAtten = 1.0;
            if (staticSpecularLightTypes[i] == 1) {
                vec3 spotDirection = SafeNormalize(
                        staticSpecularLightDirections[i],
                        vec3(0.0, -1.0, 0.0));
                vec3 fragmentDirectionFromLight = distanceToLight > 0.0001
                        ? -lightDirection
                        : spotDirection;
                float coneDot = dot(
                        spotDirection, fragmentDirectionFromLight);
                float innerConeCos = staticSpecularLightInnerConeCos[i];
                float outerConeCos = staticSpecularLightOuterConeCos[i];
                coneAtten = abs(innerConeCos - outerConeCos) > 0.0001
                        ? smoothstep(outerConeCos, innerConeCos, coneDot)
                        : step(innerConeCos, coneDot);
            } else if (staticSpecularLightTypes[i] == 2) {
                vec3 rectDirection = SafeNormalize(
                        staticSpecularLightDirections[i],
                        vec3(0.0, -1.0, 0.0));
                float frontDistance = dot(
                        fragWorldPosition - staticSpecularLightPositions[i],
                        rectDirection);
                float startFeather = staticSpecularLightStartFeathers[i];
                coneAtten = startFeather > 0.000001
                        ? smoothstep(0.0, startFeather, frontDistance)
                        : step(0.0, frontDistance);
            }
            if (coneAtten <= 0.0) continue;
            vec3 halfway = SafeNormalize(
                    viewDirection + lightDirection, worldNormal);
            float distribution = DistributionGgx(
                    worldNormal, halfway, roughness);
            float geometry = GeometrySmith(
                    worldNormal, viewDirection, lightDirection, roughness);
            vec3 fresnel = FresnelSchlick(
                    max(dot(halfway, viewDirection), 0.0), f0);
            vec3 specular = distribution * geometry * fresnel
                    / max(4.0
                            * max(dot(worldNormal, viewDirection), 0.0)
                            * ndotl,
                            0.001);
            vec3 radiance = staticSpecularLightColors[i]
                    * staticSpecularLightIntensities[i]
                    * atten
                    * coneAtten;
            staticDirectSpecular += specular * radiance * ndotl;
        }
    }

    vec3 environmentSpecular = vec3(0.0);
    if (hasEnvironment != 0 && environmentSpecularScale > 0.0) {
        vec3 reflected = reflect(-viewDirection, worldNormal);
        if (environmentBoxProjection != 0) {
            float c = cos(-environmentYaw);
            float s = sin(-environmentYaw);
            vec3 origin = fragWorldPosition - environmentInfluenceCenter;
            vec3 localOrigin = vec3(
                    origin.x * c - origin.z * s,
                    origin.y,
                    origin.x * s + origin.z * c);
            vec3 localDirection = vec3(
                    reflected.x * c - reflected.z * s,
                    reflected.y,
                    reflected.x * s + reflected.z * c);
            vec3 safeDirection = vec3(
                    abs(localDirection.x) < 0.00001 ? (localDirection.x < 0.0 ? -0.00001 : 0.00001) : localDirection.x,
                    abs(localDirection.y) < 0.00001 ? (localDirection.y < 0.0 ? -0.00001 : 0.00001) : localDirection.y,
                    abs(localDirection.z) < 0.00001 ? (localDirection.z < 0.0 ? -0.00001 : 0.00001) : localDirection.z);
            vec3 exitPlane = mix(-environmentHalfExtents, environmentHalfExtents,
                    step(vec3(0.0), localDirection));
            vec3 exitDistance = (exitPlane - localOrigin) / safeDirection;
            float distanceToBox = min(exitDistance.x,
                    min(exitDistance.y, exitDistance.z));
            vec3 localHit = localOrigin + localDirection * max(distanceToBox, 0.0);
            vec3 captureOffset = environmentCapturePosition - environmentInfluenceCenter;
            vec3 localCapture = vec3(
                    captureOffset.x * c - captureOffset.z * s,
                    captureOffset.y,
                    captureOffset.x * s + captureOffset.z * c);
            vec3 localLookup = localHit - localCapture;
            c = cos(environmentYaw);
            s = sin(environmentYaw);
            reflected = normalize(vec3(
                    localLookup.x * c - localLookup.z * s,
                    localLookup.y,
                    localLookup.x * s + localLookup.z * c));
        }
        vec3 environment = textureLod(
                environmentTexture, reflected,
                roughness * max(environmentMaxLod, 0.0)).rgb;
        vec2 environmentBrdf = EnvironmentBrdfApprox(
                roughness,
                max(dot(worldNormal, viewDirection), 0.0));
        environmentSpecular = environment
                * (f0 * environmentBrdf.x + environmentBrdf.y)
                * environmentExposure
                * environmentIntensity
                * environmentSpecularScale;
    }

    vec3 staticAtmosphericLighting = max(
            fragColor.rgb + bakedSample.rgb, vec3(0.0));
    vec3 emissiveRadiance = emissiveDecalRgb * max(decalEmissiveStrength, 0.0);
    vec3 litRgb = staticDiffuse
            + dynamicDirectDiffuse
            + dynamicDirectSpecular
            + staticDirectSpecular
            + environmentSpecular;
    vec3 surfaceOutput = litRgb * (1.0 - emissiveDecalAlpha)
            + emissiveRadiance * emissiveDecalAlpha;
    if (pbrDiagnosticMode == 1) surfaceOutput = surfaceRgb;
    else if (pbrDiagnosticMode == 2) surfaceOutput = dynamicDirectDiffuse;
    else if (pbrDiagnosticMode == 3) {
        surfaceOutput = dynamicDirectSpecular + staticDirectSpecular;
    }
    else if (pbrDiagnosticMode == 4) surfaceOutput = staticDiffuse;
    else if (pbrDiagnosticMode == 5) surfaceOutput = environmentSpecular;
    else if (pbrDiagnosticMode == 6) surfaceOutput = emissiveRadiance;
    else if (pbrDiagnosticMode == 7) surfaceOutput = vec3(1.0);
    else if (pbrDiagnosticMode == 8) {
        surfaceOutput = vec3(metallic, roughness, 0.0);
    }
    else if (pbrDiagnosticMode == 9) {
        surfaceOutput = worldNormal * 0.5 + 0.5;
    }
    else if (pbrDiagnosticMode == 10) {
        surfaceOutput = hasNormalMap != 0
                ? tangentNormalSample
                : vec3(1.0, 0.0, 1.0);
    }
    if (pbrDiagnosticMode == 0) {
        surfaceOutput = ApplySectorFog(
                surfaceOutput,
                staticAtmosphericLighting,
                fragWorldPosition);
    }
    finalColor = vec4(StoreFiniteHalfRadiance(surfaceOutput),
            clamp(baseColor.a * fragColor.a, 0.0, 1.0));
}
)";

const char* SectorDepthPrepassVs = R"(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
void main() { gl_Position = mvp * vec4(vertexPosition, 1.0); }
)";

const char* SectorDepthPrepassFs = R"(
#version 330
void main() { }
)";

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
    Shader shader = LoadShaderFromMemory(SectorLightmapVs, SectorLightmapFs);
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
            continue;
        }
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
    generatedGeometry = std::move(candidateGeometry);
    sectorCount = map.sectors.size();
    if (refreshVisibilityData) {
        visibilityGraph = std::move(candidateVisibilityGraph);
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
    localReflectionProbesCurrent = !staticObjectAdjustmentBakedDataActive
            && localReflectionProbeSurfaceHash
                    == currentSurfaceHash;
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
    localReflectionProbeSurfaceHash =
            ComputeSectorLightmapSourceHash(map);

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

    depthPrepassMaterial = LoadMaterialDefault();
    Shader depthShader = LoadShaderFromMemory(SectorDepthPrepassVs, SectorDepthPrepassFs);
    if (depthShader.id == 0) {
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

    initialized = true;
    return true;
}

void SectorMeshRenderer::Shutdown(engine::AssetManager& assets)
{
    ShutdownRendererResources(assets);
}

void SectorMeshRenderer::ShutdownRendererResources(engine::AssetManager& assets)
{
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
    localReflectionProbesCurrent = true;
    liquidRefractionFallbackLogged = false;
    atmosphereGpuFramePrepared = false;
    preGlassLightEffectsRendered = false;
    preGlassShaftApplied = false;
    preGlassHaloApplied = false;
    localReflectionProbeSurfaceHash.clear();
    staticObjectAdjustmentBakedDataActive = false;
    doorRenderer.UnloadDoorMeshes();
    UnloadSectorMeshes(meshes);
    textureHandlesById.clear();
    normalTextureHandlesById.clear();
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
    DrawScene(assets, useBakedAmbientOcclusion, runtimeObjectWorld, doorLighting, fogSettings);
}

void SectorMeshRenderer::DrawScene(
        engine::AssetManager& assets,
        bool useBakedAmbientOcclusion,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting,
        const SectorTopologyFogSettings& fogSettings,
        bool staticCaptureOnly,
        SectorUseHighlight useHighlight)
{
    if (!initialized) {
        return;
    }

    dynamicLightState.SetFlashlightCookieTexture(
            assets.GetTexture(flashlightCookieTexture));

    BeginMode3D(camera);
    skyRenderer.Draw(assets, camera);
    if (depthPrepassEnabled && depthPrepassMaterialLoaded) {
        rlDrawRenderBatchActive();
        rlColorMask(false, false, false, false);
        rlEnableDepthTest();
        rlEnableDepthMask();
        DrawDepthPrepass(assets, runtimeObjectWorld);
        rlDrawRenderBatchActive();
        rlColorMask(true, true, true, true);
    }

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
    const int pbrDiagnosticMode = static_cast<int>(
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
    for (const SectorMeshBatch& batch : meshes.sectorDrawRecords) {
        if (!ShouldDrawSectorMeshRecordForVisibility(batch, visibilityResult)) {
            continue;
        }

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
                            localReflectionProbesCurrent);
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

        const Texture2D* normalTexture = assets.GetTexture(
                NormalTextureForId(batch.materialId));

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
    if (runtimeObjectWorld != nullptr) {
        const SectorPbrEnvironmentSelection objectEnvironmentSelection =
                SelectSectorPbrEnvironment(
                        pbrEnvironment,
                        camera.position,
                        -1,
                        localReflectionProbesCurrent);
        const TextureCubemap* environmentTexture = assets.GetCubemap(
                objectEnvironmentSelection.cubemap);
        const bool environmentReady = environmentTexture != nullptr
                && environmentTexture->id != 0;
        SectorDoorDrawContext doorDrawContext;
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
        doorDrawContext.visibility = &visibilityResult;
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

        const SectorBillboardDynamicLightContext billboardLightContext = BuildBillboardDynamicLightContext();
        const TextureCubemap* pbrEnvironmentTexture = environmentReady
                ? environmentTexture : nullptr;
        staticModelRenderer.SetEnvironmentProjection(objectEnvironmentSelection);
        staticModelRenderer.Draw(
                assets,
                *runtimeObjectWorld,
                camera,
                billboardLightContext,
                staticSpecularLightState,
                surfaceLightmapBakeCurrent,
                !staticCaptureOnly && objectProbeBakeCurrent
                        && doorLighting.objectLightProbes != nullptr
                        && !doorLighting.objectLightProbes->probes.empty(),
                fogContext,
                visibilityResult,
                lightmapTextures,
                pbrEnvironmentTexture,
                useBakedAmbientOcclusion,
                renderDebugText,
                staticCaptureOnly,
                useHighlight);
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
    EndMode3D();
}

bool SectorMeshRenderer::CaptureReflectionProbe(
        engine::AssetManager& assets,
        Vector3 capturePosition,
        int resolution,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting,
        std::vector<Vector4>& outFacePixels,
        std::string& error)
{
    outFacePixels.clear();
    if (!initialized || (resolution != 64 && resolution != 128 && resolution != 256)) {
        error = "Reflection capture requires a ready renderer and a 64, 128, or 256 resolution";
        return false;
    }
    engine::RenderTarget target;
    engine::RenderTargetDescriptor descriptor;
    descriptor.debugName = "sector-reflection-probe-capture";
    descriptor.width = resolution;
    descriptor.height = resolution;
    descriptor.colorFormat = engine::RenderTargetColorFormat::Rgba16Float;
    descriptor.filter = engine::RenderTargetFilter::Bilinear;
    descriptor.wrap = engine::RenderTargetWrap::Clamp;
    descriptor.depth = engine::RenderTargetDepthKind::Renderbuffer;
    if (!engine::LoadRenderTarget(descriptor, target, &error)) return false;

    const SectorViewPose savedPose = RendererPose();
    const float savedFov = verticalFovDegrees;
    const bool savedDynamicLighting = dynamicLightingEnabled;
    const SectorPbrContributionSettings savedPbr = pbrContributionSettings;
    const RuntimePortalVisibilityResult savedVisibility = visibilityResult;
    dynamicLightingEnabled = false;
    SectorPbrContributionSettings capturePbr = savedPbr;
    capturePbr.worldEnvironmentSpecularScale = 0.0f;
    capturePbr.diagnosticMode = SectorPbrDiagnosticMode::Full;
    SetPbrContributionSettings(capturePbr);
    SetVerticalFovDegrees(90.0f);

    const std::array<SectorViewPose, 6> poses{{
            {capturePosition, 0.0f, 0.0f, 0.0f},
            {capturePosition, PI, 0.0f, 0.0f},
            {capturePosition, PI * 0.5f, PI * 0.5f, 0.0f},
            {capturePosition, PI * 0.5f, -PI * 0.5f, 0.0f},
            {capturePosition, PI * 0.5f, 0.0f, 0.0f},
            {capturePosition, -PI * 0.5f, 0.0f, 0.0f}}};
    const std::size_t facePixelCount = static_cast<std::size_t>(resolution)
            * static_cast<std::size_t>(resolution);
    std::vector<float> readback(facePixelCount * 4u);
    outFacePixels.resize(facePixelCount * 6u);
    const SectorTopologyFogSettings noFog{};
    bool succeeded = true;
    for (int face = 0; face < 6; ++face) {
        ApplyRendererPose(poses[static_cast<std::size_t>(face)], true);
        BeginTextureMode(target.native);
        ClearBackground(BLACK);
        DrawScene(assets, true, runtimeObjectWorld, doorLighting, noFog, true);
        rlDrawRenderBatchActive();
        glReadPixels(0, 0, resolution, resolution, GL_RGBA, GL_FLOAT, readback.data());
        const GLenum glError = glGetError();
        EndTextureMode();
        if (glError != GL_NO_ERROR) {
            error = "OpenGL readback failed while capturing reflection probe";
            succeeded = false;
            break;
        }
        // The preview camera basis is mirrored relative to OpenGL cubemap face
        // coordinates; convert the framebuffer image before prefiltering/upload.
        for (int y = 0; y < resolution; ++y) {
            const int sourceY = resolution - 1 - y;
            for (int x = 0; x < resolution; ++x) {
                const int sourceX = resolution - 1 - x;
                const std::size_t source = static_cast<std::size_t>(
                        (sourceY * resolution + sourceX) * 4);
                const std::size_t destination = static_cast<std::size_t>(face)
                                * facePixelCount
                        + static_cast<std::size_t>(y * resolution + x);
                outFacePixels[destination] = Vector4{
                        readback[source], readback[source + 1],
                        readback[source + 2], readback[source + 3]};
            }
        }
    }
    ApplyRendererPose(savedPose, false);
    SetVerticalFovDegrees(savedFov);
    dynamicLightingEnabled = savedDynamicLighting;
    SetPbrContributionSettings(savedPbr);
    visibilityResult = savedVisibility;
    engine::UnloadRenderTarget(target);
    if (!succeeded) {
        outFacePixels.clear();
        return false;
    }
    error.clear();
    return true;
}

void SectorMeshRenderer::DrawDepthPrepass(
        engine::AssetManager& assets,
        engine::World* runtimeObjectWorld)
{
    for (const SectorMeshBatch& batch : meshes.sectorDrawRecords) {
        if (batch.alphaTest
                || !ShouldDrawSectorMeshRecordForVisibility(batch, visibilityResult)) {
            continue;
        }
        DrawMesh(batch.mesh, depthPrepassMaterial, MatrixIdentity());
    }
    if (runtimeObjectWorld == nullptr) return;

    SectorDynamicSpotLightShadowRenderContext context;
    context.assets = &assets;
    context.sectorDrawRecords = &meshes.sectorDrawRecords;
    doorRenderer.PrepareShadowRenderContext(context, runtimeObjectWorld);
    staticModelRenderer.PrepareShadowRenderContext(context, runtimeObjectWorld);

    if (context.doorShadowCasters != nullptr && context.doorMeshResolver != nullptr) {
        for (const SectorDoorShadowCaster& caster : *context.doorShadowCasters) {
            float width = 0.0f;
            float height = 0.0f;
            const Mesh* mesh = context.doorMeshResolver(
                    context.doorMeshResolverUserData, caster, width, height);
            if (mesh == nullptr || mesh->vertexCount <= 0) continue;
            DrawMesh(*mesh, depthPrepassMaterial,
                    BuildSectorDoorShadowCasterModelMatrix(caster, width, height));
        }
    }
    if (context.doorModelShadowCasters != nullptr) {
        for (const SectorDoorModelShadowCaster& caster : *context.doorModelShadowCasters) {
            const engine::ModelAsset* asset = assets.GetModelAsset(caster.model);
            if (asset == nullptr) continue;
            const Matrix transform = MatrixMultiply(asset->model.transform, caster.transform);
            for (int i = 0; i < asset->model.meshCount; ++i) {
                if (asset->model.meshes[i].vertexCount > 0) {
                    DrawMesh(asset->model.meshes[i], depthPrepassMaterial, transform);
                }
            }
        }
    }
    if (context.staticModelShadowCasters != nullptr) {
        rlDisableBackfaceCulling();
        for (const SectorStaticModelShadowCaster& caster : *context.staticModelShadowCasters) {
            const engine::ModelAsset* asset = assets.GetModelAsset(caster.model);
            if (asset == nullptr) continue;
            const Matrix transform = MatrixMultiply(asset->model.transform, caster.transform);
            for (int i = 0; i < asset->model.meshCount; ++i) {
                if (asset->model.meshes[i].vertexCount > 0) {
                    DrawMesh(asset->model.meshes[i], depthPrepassMaterial, transform);
                }
            }
        }
        rlEnableBackfaceCulling();
    }
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
    const SectorPbrEnvironmentSelection viewmodelEnvironment =
            SelectSectorPbrEnvironment(
                    pbrEnvironment,
                    viewmodelCamera.position,
                    receiverSectorId,
                    localReflectionProbesCurrent);
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
    staticModelRenderer.SetEnvironmentProjection(viewmodelEnvironment);
    staticModelRenderer.DrawViewmodel(
            asset, instance, viewmodelCamera, transform,
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

bool SectorMeshRenderer::EnsureHdrCompositeShader()
{
    if (hdrCompositeShader.id != 0) return true;
    if (hdrCompositeShaderFailed) return false;
    hdrCompositeShader = LoadShaderFromMemory(HdrCompositeVs, HdrCompositeFs);
    if (hdrCompositeShader.id == 0) { hdrCompositeShaderFailed=true; return false; }
    hdrCompositeSceneLoc = GetShaderLocation(hdrCompositeShader, "sceneColor");
    hdrCompositeSourceLoc = GetShaderLocation(hdrCompositeShader, "sourceColor");
    hdrCompositeModeLoc = GetShaderLocation(hdrCompositeShader, "compositeMode");
    return hdrCompositeSceneLoc >= 0 && hdrCompositeSourceLoc >= 0
            && hdrCompositeModeLoc >= 0;
}

bool SectorMeshRenderer::CommitHdrScratch(engine::RenderTarget& sceneTarget)
{
    if (!EnsureHdrCompositeShader()) return false;
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
    if (!initialized || !EnsureHdrSceneScratch(sceneTarget) || !EnsureHdrCompositeShader()
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

    const bool visibleWindows = runtimeObjectWorld != nullptr
            && windowRenderer.HasVisibleWindows(*runtimeObjectWorld, &visibilityResult);
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
    }

    if (!visibleWindows && !visibleLiquids) {
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
        liquidContext.localReflectionProbesCurrent = localReflectionProbesCurrent;
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
        windowContext.assets = &assets;
        windowContext.world = runtimeObjectWorld;
        windowContext.camera = camera;
        windowContext.visibility = &visibilityResult;
        windowContext.environment = &pbrEnvironment;
        windowContext.localReflectionProbesCurrent = localReflectionProbesCurrent;
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
        staticObjectAdjustmentOriginalLocalReflectionProbesCurrent =
                localReflectionProbesCurrent;
        staticObjectAdjustmentBakedDataActive = true;
    }
    if (static_cast<SectorLightmapStatus>(lightmapStatus)
            == SectorLightmapStatus::Valid) {
        lightmapStatus = static_cast<int>(SectorLightmapStatus::Stale);
    }
    surfaceLightmapBakeCurrent = false;
    objectProbeBakeCurrent = false;
    localReflectionProbesCurrent = false;
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
        localReflectionProbesCurrent =
                staticObjectAdjustmentOriginalLocalReflectionProbesCurrent;
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
                dynamicPortalBlockers);
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
