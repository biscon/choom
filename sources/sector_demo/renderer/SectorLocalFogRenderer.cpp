#include "sector_demo/renderer/SectorLocalFogRenderer.h"

#include "engine/render/ColorTransfer.h"
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <string>

namespace game {
namespace {

constexpr int MaxLocalFogVolumes = 16;
constexpr SectorLocalFogPathLimitSettings LocalFogPathLimitSettings{};

const char* FullscreenVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragUv;
uniform mat4 mvp;
void main() {
    fragUv = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* AccumulateFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;

float StoreFiniteHalfChannel(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? 65504.0 : 0.0;
    return min(max(value, 0.0), 65504.0);
}
vec3 StoreFiniteHalfRadiance(vec3 value) {
    return vec3(StoreFiniteHalfChannel(value.r), StoreFiniteHalfChannel(value.g),
            StoreFiniteHalfChannel(value.b));
}
float StoreBoundedAlpha(float value) {
    return (isnan(value) || isinf(value)) ? 0.0 : clamp(value, 0.0, 1.0);
}
uniform sampler2D sceneDepth;
uniform int volumeCount;
uniform int marchSteps;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform float nearPlane;
uniform float farPlane;
uniform float runtimeSeconds;
uniform vec3 pathLimitSettings;
uniform float probeFootprintFraction;
uniform vec3 fogCenters[16];
uniform vec3 fogRadii[16];
uniform vec3 fogColors[16];
uniform vec4 fogParamsA[16];
uniform vec4 fogParamsB[16];
uniform vec4 fogLightingA[16];
uniform vec4 fogLightingB[16];
uniform vec4 fogLightingC[16];

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
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform int shadowAtlasTilesPerRow;
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

const vec2 kFogShadowDisk[4] = vec2[4](
    vec2(-0.707, -0.707),
    vec2( 0.707, -0.707),
    vec2(-0.707,  0.707),
    vec2( 0.707,  0.707)
);

float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 p) {
    vec3 cell = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(cell + vec3(0,0,0));
    float n100 = hash31(cell + vec3(1,0,0));
    float n010 = hash31(cell + vec3(0,1,0));
    float n110 = hash31(cell + vec3(1,1,0));
    float n001 = hash31(cell + vec3(0,0,1));
    float n101 = hash31(cell + vec3(1,0,1));
    float n011 = hash31(cell + vec3(0,1,1));
    float n111 = hash31(cell + vec3(1,1,1));
    return mix(mix(mix(n000,n100,f.x), mix(n010,n110,f.x), f.y),
               mix(mix(n001,n101,f.x), mix(n011,n111,f.x), f.y), f.z);
}

bool intersectEllipsoid(vec3 origin, vec3 direction, vec3 center, vec3 radii, out float enterT, out float exitT) {
    vec3 inverseRadii = 1.0 / max(radii, vec3(0.0001));
    vec3 o = (origin - center) * inverseRadii;
    vec3 d = direction * inverseRadii;
    float a = dot(d, d);
    float b = 2.0 * dot(o, d);
    float c = dot(o, o) - 1.0;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0 || a <= 0.0) return false;
    float root = sqrt(discriminant);
    enterT = (-b - root) / (2.0 * a);
    exitT = (-b + root) / (2.0 * a);
    return exitT > max(enterT, 0.0);
}

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSq = dot(value, value);
    return lengthSq > 0.00000001 ? value * inversesqrt(lengthSq) : fallback;
}

float sampleShadowMap(int shadowSlot, vec2 uv) {
    int tiles=max(shadowAtlasTilesPerRow,1); vec2 tile=vec2(shadowSlot%tiles,shadowSlot/tiles);
    return texture(shadowMap0,(tile+clamp(uv,vec2(0.001),vec2(0.999)))/float(tiles)).r;
}

float dynamicLightShadowVisibility(int lightIndex, int shadowSlot, vec3 worldPosition) {
    if (shadowSlot < 0 || shadowSlot >= MAX_DYNAMIC_SHADOW_CASTERS) return 1.0;
    vec3 shadowCoord;
    if(dynamicLightTypes[lightIndex]==0) { vec3 fromLight=worldPosition-dynamicLightPositions[lightIndex];
        float radial=length(fromLight); if(radial<=0.00001)return 1.0; shadowSlot+=fromLight.z>=0.0?0:1;
        shadowCoord=vec3(fromLight.xy/max(radial+abs(fromLight.z),0.00001)*0.5+0.5,
                radial/max(dynamicLightRadii[lightIndex],0.00001)); }
    else { vec3 fromLight=worldPosition-dynamicLightPositions[lightIndex];
        vec3 forward=safeNormalize(dynamicLightDirections[lightIndex],vec3(0,-1,0));
        vec3 upReference=abs(forward.y)>0.98?vec3(0,0,1):vec3(0,1,0);
        vec3 right=safeNormalize(cross(forward,upReference),vec3(1,0,0)); vec3 up=cross(right,forward);
        float z=dot(fromLight,forward); if(z<=0.05)return 1.0;
        float tangent=tan(min(acos(clamp(dynamicLightOuterConeCos[lightIndex],-0.999,0.999)),1.553343));
        float farPlane=dynamicLightRadii[lightIndex];
        float ndc=(farPlane+0.05)/(farPlane-0.05)-(2.0*farPlane*0.05)/((farPlane-0.05)*z);
        shadowCoord=vec3(vec2(dot(fromLight,right),dot(fromLight,up))/max(2.0*z*tangent,0.00001)+0.5,ndc*0.5+0.5); }
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0
            || shadowCoord.y < 0.0 || shadowCoord.y > 1.0
            || shadowCoord.z < 0.0 || shadowCoord.z > 1.0) return 1.0;

    float compareDepth = shadowCoord.z - min(max(shadowBias[shadowSlot], 0.0), 0.02);
    float softness = clamp(shadowSoftness[shadowSlot], 0.0, 8.0);
    if (softness <= 0.0) {
        return compareDepth <= sampleShadowMap(shadowSlot, shadowCoord.xy) ? 1.0 : 0.0;
    }

    vec2 texelSize = vec2(float(max(shadowAtlasTilesPerRow,1))) / vec2(textureSize(shadowMap0, 0));
    vec2 radius = max(0.25, softness) * texelSize;
    float visible = 0.0;
    for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
        vec2 sampleUv = clamp(
                shadowCoord.xy + kFogShadowDisk[sampleIndex] * radius,
                vec2(0.0),
                vec2(1.0));
        visible += compareDepth <= sampleShadowMap(shadowSlot, sampleUv) ? 1.0 : 0.0;
    }
    return visible * 0.25;
}

vec3 evaluateDynamicLighting(vec3 worldPosition) {
    vec3 dynamicDirect = vec3(0.0);
    for (int lightIndex = 0; lightIndex < MAX_DYNAMIC_LIGHTS; ++lightIndex) {
        if (lightIndex >= dynamicLightCount) break;
        float radius = dynamicLightRadii[lightIndex];
        vec3 toLight = dynamicLightPositions[lightIndex] - worldPosition;
        float distanceSq = dot(toLight, toLight);
        if (radius <= 0.0 || distanceSq >= radius * radius) continue;

        float distanceToLight = sqrt(max(distanceSq, 0.0));
        vec3 lightDirection = distanceToLight > 0.0001
                ? toLight / distanceToLight
                : vec3(0.0, 1.0, 0.0);
        float attenuation = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
        attenuation *= attenuation;
        float coneAttenuation = 1.0;
        if (dynamicLightTypes[lightIndex] == 1) {
            vec3 spotDirection = safeNormalize(
                    dynamicLightDirections[lightIndex],
                    vec3(0.0, -1.0, 0.0));
            vec3 sampleDirectionFromLight = distanceToLight > 0.0001
                    ? -lightDirection
                    : spotDirection;
            float coneDot = dot(spotDirection, sampleDirectionFromLight);
            float innerConeCos = dynamicLightInnerConeCos[lightIndex];
            float outerConeCos = dynamicLightOuterConeCos[lightIndex];
            coneAttenuation = abs(innerConeCos - outerConeCos) > 0.0001
                    ? smoothstep(outerConeCos, innerConeCos, coneDot)
                    : step(innerConeCos, coneDot);
        }
        int shadowSlot = dynamicLightShadowSlots[lightIndex];
        if (shadowSlot >= 0 && coneAttenuation > 0.0) {
            float visibility = dynamicLightShadowVisibility(lightIndex, shadowSlot, worldPosition);
            coneAttenuation *= mix(1.0, visibility,
                    clamp(shadowStrength[shadowSlot], 0.0, 1.0));
        }
        dynamicDirect += dynamicLightColors[lightIndex]
                * dynamicLightIntensities[lightIndex]
                * attenuation
                * coneAttenuation;
    }
    return dynamicDirect;
}

void unpackStaticLighting(
        int volumeIndex,
        out vec3 negativeXNegativeZ,
        out vec3 positiveXNegativeZ,
        out vec3 negativeXPositiveZ,
        out vec3 positiveXPositiveZ) {
    vec4 packedA = fogLightingA[volumeIndex];
    vec4 packedB = fogLightingB[volumeIndex];
    vec4 packedC = fogLightingC[volumeIndex];
    negativeXNegativeZ = packedA.rgb;
    positiveXNegativeZ = vec3(packedA.a, packedB.xy);
    negativeXPositiveZ = vec3(packedB.zw, packedC.x);
    positiveXPositiveZ = packedC.yzw;
}

vec3 interpolateStaticLighting(int volumeIndex, vec2 normalizedLocalXZ) {
    vec3 negativeXNegativeZ;
    vec3 positiveXNegativeZ;
    vec3 negativeXPositiveZ;
    vec3 positiveXPositiveZ;
    unpackStaticLighting(
            volumeIndex,
            negativeXNegativeZ,
            positiveXNegativeZ,
            negativeXPositiveZ,
            positiveXPositiveZ);
    float footprint = max(probeFootprintFraction, 0.0001);
    vec2 uv = clamp(normalizedLocalXZ / (2.0 * footprint) + 0.5, vec2(0.0), vec2(1.0));
    vec3 negativeZ = mix(negativeXNegativeZ, positiveXNegativeZ, uv.x);
    vec3 positiveZ = mix(negativeXPositiveZ, positiveXPositiveZ, uv.x);
    return mix(negativeZ, positiveZ, uv.y);
}

float effectiveOpticalPathLength(float geometricLength, float volumeHeight) {
    float minimumPath = max(pathLimitSettings.x, 0.0001);
    float heightMultiplier = max(pathLimitSettings.y, 0.0);
    float saturationPower = max(pathLimitSettings.z, 1.0);
    float pathLimit = max(minimumPath, max(volumeHeight, 0.0) * heightMultiplier);
    float ratio = geometricLength / pathLimit;
    float denominator = pow(1.0 + pow(ratio, saturationPower), 1.0 / saturationPower);
    return min(geometricLength / max(denominator, 1.0), pathLimit);
}

void main() {
    float depth = texture(sceneDepth, fragUv).r;
    vec2 ndc = fragUv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float sceneDistance = depth >= 0.999999
            ? farPlane
            : forwardDistance / max(dot(rayDirection, cameraForward), 0.0001);

    float totalOpticalDepth = 0.0;
    vec3 weightedColor = vec3(0.0);
    for (int volumeIndex = 0; volumeIndex < 16; ++volumeIndex) {
        if (volumeIndex >= volumeCount) break;
        float enterT = 0.0;
        float exitT = 0.0;
        if (!intersectEllipsoid(cameraPosition, rayDirection, fogCenters[volumeIndex], fogRadii[volumeIndex], enterT, exitT)) continue;
        enterT = max(enterT, 0.0);
        exitT = min(exitT, sceneDistance);
        if (exitT <= enterT) continue;

        float segmentLength = exitT - enterT;
        float geometricStepLength = segmentLength / float(max(marchSteps, 1));
        float volumeHeight = fogRadii[volumeIndex].y * 2.0;
        float effectiveStepLength = effectiveOpticalPathLength(segmentLength, volumeHeight)
                / float(max(marchSteps, 1));
        float volumeOpticalDepth = 0.0;
        vec3 volumeWeightedScattering = vec3(0.0);
        vec4 paramsA = fogParamsA[volumeIndex];
        vec4 paramsB = fogParamsB[volumeIndex];
        float noiseModulation = 1.0;
        if (paramsB.x > 0.0001) {
            vec3 noiseSamplePosition = cameraPosition
                    + rayDirection * ((enterT + exitT) * 0.5);
            vec2 flowWorld = vec2(cos(paramsB.y), sin(paramsB.y))
                    * paramsB.z * runtimeSeconds;
            noiseSamplePosition.xz -= flowWorld;
            float coherentNoise = smoothstep(
                    0.15,
                    0.85,
                    valueNoise(noiseSamplePosition / max(paramsA.w, 0.05)));
            noiseModulation = mix(1.0, 2.0 * coherentNoise, paramsB.x);
        }
        for (int stepIndex = 0; stepIndex < 12; ++stepIndex) {
            if (stepIndex >= marchSteps) break;
            float t = enterT + (float(stepIndex) + 0.5) * geometricStepLength;
            vec3 position = cameraPosition + rayDirection * t;
            vec3 local = (position - fogCenters[volumeIndex]) / fogRadii[volumeIndex];
            float normalizedRadius = length(local);
            float softness = max(paramsA.z, 0.0001);
            float boundary = 1.0 - smoothstep(1.0 - softness, 1.0, normalizedRadius);
            float sampleOpticalDepth = paramsA.x * boundary * effectiveStepLength;
            if (sampleOpticalDepth <= 0.0) continue;
            vec3 staticLighting = interpolateStaticLighting(volumeIndex, local.xz);
            vec3 sampleLighting = max(
                    staticLighting + evaluateDynamicLighting(position),
                    vec3(0.0));
            volumeOpticalDepth += sampleOpticalDepth;
            volumeWeightedScattering += fogColors[volumeIndex]
                    * sampleLighting
                    * sampleOpticalDepth;
        }
        float capOpticalDepth = -log(max(1.0 - clamp(paramsA.y, 0.0, 0.9999), 0.0001));
        float modulatedOpticalDepth = volumeOpticalDepth * noiseModulation;
        float cappedOpticalDepth = min(modulatedOpticalDepth, capOpticalDepth);
        float capScale = volumeOpticalDepth > 0.00001
                ? cappedOpticalDepth / volumeOpticalDepth
                : 0.0;
        totalOpticalDepth += cappedOpticalDepth;
        weightedColor += volumeWeightedScattering * capScale;
    }
    float opacity = 1.0 - exp(-totalOpticalDepth);
    vec3 color = totalOpticalDepth > 0.00001 ? weightedColor / totalOpticalDepth : vec3(0.0);
    vec3 premultipliedRadiance = StoreFiniteHalfRadiance(color * opacity);
    finalColor = vec4(premultipliedRadiance, StoreBoundedAlpha(opacity));
}
)";

const char* CompositeFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D fogTexture;
uniform vec2 fogTexelSize;
uniform int bilateralUpsample;

float SanitizeIntermediateChannel(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? 65504.0 : 0.0;
    return max(value, 0.0);
}
vec3 SanitizeIntermediateRadiance(vec3 value) {
    return vec3(SanitizeIntermediateChannel(value.r),
            SanitizeIntermediateChannel(value.g),
            SanitizeIntermediateChannel(value.b));
}
float SanitizeOpacity(float value) {
    return (isnan(value) || isinf(value)) ? 0.0 : clamp(value, 0.0, 1.0);
}

void main() {
    vec4 fog = texture(fogTexture, fragUv);
    if (bilateralUpsample != 0) {
        float centerDepth = texture(sceneDepth, fragUv).r;
        vec4 accumulated = vec4(0.0);
        float totalWeight = 0.0;
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                if (x != 0 && y != 0) continue;
                vec2 sampleUv = clamp(fragUv + vec2(x, y) * fogTexelSize, vec2(0.0), vec2(1.0));
                float sampleDepth = texture(sceneDepth, sampleUv).r;
                float weight = exp(-abs(sampleDepth - centerDepth) * 600.0);
                accumulated += texture(fogTexture, sampleUv) * weight;
                totalWeight += weight;
            }
        }
        fog = accumulated / max(totalWeight, 0.0001);
    }
    vec4 scene = texture(sceneColor, fragUv);
    vec3 composed = SanitizeIntermediateRadiance(scene.rgb)
            * (1.0 - SanitizeOpacity(fog.a))
            + SanitizeIntermediateRadiance(fog.rgb);
    composed = SanitizeIntermediateRadiance(composed);
    finalColor = vec4(composed,
            (isnan(scene.a) || isinf(scene.a)) ? 1.0 : clamp(scene.a, 0.0, 1.0));
}
)";

Rectangle SourceRect(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f, static_cast<float>(texture.width), -static_cast<float>(texture.height)};
}

Rectangle DestinationRect(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
}

bool ShaderReady(Shader shader)
{
    return shader.id != 0;
}

int GetShaderLocationArrayBase(Shader shader, const char* name)
{
    const int location = GetShaderLocation(shader, name);
    if (location >= 0) return location;
    const std::string indexedName = std::string(name) + "[0]";
    return GetShaderLocation(shader, indexedName.c_str());
}

int GetShaderLocationArrayElement(Shader shader, const char* name, std::size_t index)
{
    const std::string indexedName = std::string(name) + "[" + std::to_string(index) + "]";
    return GetShaderLocation(shader, indexedName.c_str());
}

bool SameVector3(Vector3 a, Vector3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool SameColor(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

} // namespace

bool SectorLocalFogRenderer::EnsureShaders()
{
    if (ShaderReady(accumulateShader) && ShaderReady(compositeShader)) return true;
    if (shaderFailed) return false;
    Shutdown();
    accumulateShader = LoadShaderFromMemory(FullscreenVs, AccumulateFs);
    compositeShader = LoadShaderFromMemory(FullscreenVs, CompositeFs);
    if (!ShaderReady(accumulateShader) || !ShaderReady(compositeShader)) {
        shaderFailed = true;
        accumulationDiagnostic = "disabled: shader unavailable";
        return false;
    }

    accumulateLocations.sceneDepth = GetShaderLocation(accumulateShader, "sceneDepth");
    accumulateLocations.volumeCount = GetShaderLocation(accumulateShader, "volumeCount");
    accumulateLocations.marchSteps = GetShaderLocation(accumulateShader, "marchSteps");
    accumulateLocations.cameraPosition = GetShaderLocation(accumulateShader, "cameraPosition");
    accumulateLocations.cameraForward = GetShaderLocation(accumulateShader, "cameraForward");
    accumulateLocations.cameraRight = GetShaderLocation(accumulateShader, "cameraRight");
    accumulateLocations.cameraUp = GetShaderLocation(accumulateShader, "cameraUp");
    accumulateLocations.tanHalfFov = GetShaderLocation(accumulateShader, "tanHalfFov");
    accumulateLocations.aspectRatio = GetShaderLocation(accumulateShader, "aspectRatio");
    accumulateLocations.nearPlane = GetShaderLocation(accumulateShader, "nearPlane");
    accumulateLocations.farPlane = GetShaderLocation(accumulateShader, "farPlane");
    accumulateLocations.runtimeSeconds = GetShaderLocation(accumulateShader, "runtimeSeconds");
    accumulateLocations.pathLimitSettings = GetShaderLocation(accumulateShader, "pathLimitSettings");
    accumulateLocations.probeFootprintFraction =
            GetShaderLocation(accumulateShader, "probeFootprintFraction");
    accumulateLocations.fogCenters = GetShaderLocationArrayBase(accumulateShader, "fogCenters");
    accumulateLocations.fogRadii = GetShaderLocationArrayBase(accumulateShader, "fogRadii");
    accumulateLocations.fogColors = GetShaderLocationArrayBase(accumulateShader, "fogColors");
    accumulateLocations.fogParamsA = GetShaderLocationArrayBase(accumulateShader, "fogParamsA");
    accumulateLocations.fogParamsB = GetShaderLocationArrayBase(accumulateShader, "fogParamsB");
    accumulateLocations.fogLightingA = GetShaderLocationArrayBase(accumulateShader, "fogLightingA");
    accumulateLocations.fogLightingB = GetShaderLocationArrayBase(accumulateShader, "fogLightingB");
    accumulateLocations.fogLightingC = GetShaderLocationArrayBase(accumulateShader, "fogLightingC");
    accumulateLocations.dynamicLights.dynamicLightCount =
            GetShaderLocation(accumulateShader, "dynamicLightCount");
    accumulateLocations.dynamicLights.dynamicLightPositions =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightPositions");
    accumulateLocations.dynamicLights.dynamicLightColors =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightColors");
    accumulateLocations.dynamicLights.dynamicLightRadii =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightRadii");
    accumulateLocations.dynamicLights.dynamicLightIntensities =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightIntensities");
    accumulateLocations.dynamicLights.dynamicLightTypes =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightTypes");
    accumulateLocations.dynamicLights.dynamicLightDirections =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightDirections");
    accumulateLocations.dynamicLights.dynamicLightInnerConeCos =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightInnerConeCos");
    accumulateLocations.dynamicLights.dynamicLightOuterConeCos =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightOuterConeCos");
    accumulateLocations.dynamicShadows.dynamicLightShadowSlots =
            GetShaderLocationArrayBase(accumulateShader, "dynamicLightShadowSlots");
    for (std::size_t index = 0; index < MaxDynamicSpotLightShadowCasters; ++index) {
        accumulateLocations.dynamicShadows.shadowLightMatrices[index] =
                GetShaderLocationArrayElement(accumulateShader, "shadowLightMatrices", index);
    }
    accumulateLocations.dynamicShadows.shadowBias =
            GetShaderLocationArrayBase(accumulateShader, "shadowBias");
    accumulateLocations.dynamicShadows.shadowStrength =
            GetShaderLocationArrayBase(accumulateShader, "shadowStrength");
    accumulateLocations.dynamicShadows.shadowSoftness =
            GetShaderLocationArrayBase(accumulateShader, "shadowSoftness");
    accumulateLocations.dynamicShadows.shadowAtlasTilesPerRow =
            GetShaderLocation(accumulateShader, "shadowAtlasTilesPerRow");
    accumulateLocations.shadowMap0 = GetShaderLocation(accumulateShader, "shadowMap0");
    accumulateLocations.shadowMap1 = GetShaderLocation(accumulateShader, "shadowMap1");

    compositeLocations.sceneColor = GetShaderLocation(compositeShader, "sceneColor");
    compositeLocations.sceneDepth = GetShaderLocation(compositeShader, "sceneDepth");
    compositeLocations.fogTexture = GetShaderLocation(compositeShader, "fogTexture");
    compositeLocations.fogTexelSize = GetShaderLocation(compositeShader, "fogTexelSize");
    compositeLocations.bilateralUpsample = GetShaderLocation(compositeShader, "bilateralUpsample");
    return true;
}

void SectorLocalFogRenderer::ReleaseTargets()
{
    engine::UnloadRenderTarget(fogTarget);
    sceneWidth = 0;
    sceneHeight = 0;
    targetScale = 0.0f;
}

bool SectorLocalFogRenderer::EnsureTargets(int width, int height, float scale)
{
    if (engine::IsRenderTargetReady(fogTarget)
            && sceneWidth == width && sceneHeight == height && targetScale == scale) return true;
    if (failedWidth == width && failedHeight == height && failedScale == scale) return false;
    ReleaseTargets();
    const int fogWidth = std::max(1, static_cast<int>(std::round(width * scale)));
    const int fogHeight = std::max(1, static_cast<int>(std::round(height * scale)));
    std::string error;
    // RGB is premultiplied linear HDR in-scattered radiance; alpha is bounded
    // opacity (one minus transmittance).
    engine::LoadRenderTarget(
            engine::RenderTargetDescriptor{
                    "local-fog-accumulation",
                    fogWidth,
                    fogHeight,
                    engine::RenderTargetColorFormat::Rgba16Float,
                    engine::RenderTargetFilter::Bilinear,
                    engine::RenderTargetWrap::Clamp,
                    engine::RenderTargetDepthKind::None,
                    1},
            fogTarget,
            &error);
    if (!engine::IsRenderTargetReady(fogTarget)) {
        failedWidth = width;
        failedHeight = height;
        failedScale = scale;
        ReleaseTargets();
        accumulationDiagnostic = "disabled: " + error;
        return false;
    }
    failedWidth = 0;
    failedHeight = 0;
    failedScale = 0.0f;
    sceneWidth = width;
    sceneHeight = height;
    targetScale = scale;
    accumulationDiagnostic = engine::FormatRenderTargetDiagnostic(fogTarget);
    return true;
}

void SectorLocalFogRenderer::ClearStaticLightingCache()
{
    for (StaticLightingCacheEntry& entry : staticLightingCache) {
        entry = StaticLightingCacheEntry{};
    }
}

void SectorLocalFogRenderer::RefreshStaticLightingCacheIdentity(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes)
{
    const SectorBakedObjectLightProbe* probeData = objectLightProbes.probes.empty()
            ? nullptr
            : objectLightProbes.probes.data();
    const std::size_t probeSourceHashValue =
            std::hash<std::string>{}(objectLightProbes.metadata.sourceHash);
    const std::size_t mapProbeSourceHashValue =
            std::hash<std::string>{}(map.bakedLightmap.objectProbes.sourceHash);
    if (cachedProbeData == probeData
            && cachedProbeCount == objectLightProbes.probes.size()
            && cachedProbeSourceHashValue == probeSourceHashValue
            && cachedMapProbeSourceHashValue == mapProbeSourceHashValue) {
        return;
    }

    ClearStaticLightingCache();
    cachedProbeData = probeData;
    cachedProbeCount = objectLightProbes.probes.size();
    cachedProbeSourceHashValue = probeSourceHashValue;
    cachedMapProbeSourceHashValue = mapProbeSourceHashValue;
}

const SectorLocalFogStaticLightingSamples& SectorLocalFogRenderer::StaticLightingForVolume(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorCompiledLocalFogVolume& volume)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, volume.topologySectorId);
    const Color sectorAmbientColor = sector != nullptr ? sector->ambientColor : Color{};
    const float sectorAmbientIntensity = sector != nullptr ? sector->ambientIntensity : 0.0f;
    StaticLightingCacheEntry* available = nullptr;
    for (StaticLightingCacheEntry& entry : staticLightingCache) {
        if (entry.valid && entry.sourceFogVolumeId == volume.sourceAuthoringFogVolumeId) {
            if (entry.topologySectorId == volume.topologySectorId
                    && SameVector3(entry.centerWorld, volume.centerWorld)
                    && SameVector3(entry.radiiWorld, volume.radiiWorld)
                    && SameColor(entry.sectorAmbientColor, sectorAmbientColor)
                    && entry.sectorAmbientIntensity == sectorAmbientIntensity) {
                return entry.samples;
            }
            available = &entry;
            break;
        }
        if (!entry.valid && available == nullptr) available = &entry;
    }

    if (available == nullptr) {
        const std::size_t slot = static_cast<std::size_t>(
                std::max(volume.sourceAuthoringFogVolumeId, 0)) % staticLightingCache.size();
        available = &staticLightingCache[slot];
    }
    available->valid = true;
    available->sourceFogVolumeId = volume.sourceAuthoringFogVolumeId;
    available->topologySectorId = volume.topologySectorId;
    available->centerWorld = volume.centerWorld;
    available->radiiWorld = volume.radiiWorld;
    available->sectorAmbientColor = sectorAmbientColor;
    available->sectorAmbientIntensity = sectorAmbientIntensity;
    available->samples = SampleSectorLocalFogStaticLighting(map, objectLightProbes, volume);
    return available->samples;
}

bool SectorLocalFogRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& sceneScratch,
        const SectorTopologyMap& map,
        SectorVolumetricQuality quality,
        const Camera3D& camera,
        float runtimeSeconds,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorBillboardDynamicLightContext& dynamicLightContext)
{
    eligibleVolumeCount = 0;
    activeVolumeCount = 0;
    if (sceneTarget.texture.id == 0 || sceneTarget.depth.id == 0 || sceneTarget.depth.mipmaps <= 0
            || quality == SectorVolumetricQuality::Off) return false;

    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane) {
        if (!warnedInvalidProjection) {
            TraceLog(
                    LOG_WARNING,
                    "LOCAL FOG: invalid camera clip planes (near %.3f, far %.3f); local fog disabled",
                    nearPlane,
                    farPlane);
            warnedInvalidProjection = true;
        }
        return false;
    }

    float scale = 0.5f;
    int steps = 8;
    int cap = 8;
    if (quality == SectorVolumetricQuality::Low) {
        scale = 0.25f; steps = 4; cap = 4;
    } else if (quality == SectorVolumetricQuality::High) {
        scale = 1.0f; steps = 12; cap = 16;
    }
    if (!EnsureShaders() || !EnsureTargets(sceneTarget.texture.width, sceneTarget.texture.height, scale)) {
        if (!warnedUnavailable) {
            TraceLog(LOG_WARNING, "LOCAL FOG: post-process resources unavailable; local fog disabled");
            warnedUnavailable = true;
        }
        return false;
    }
    RefreshStaticLightingCacheIdentity(map, objectLightProbes);

    std::array<const SectorCompiledLocalFogVolume*, MaxLocalFogVolumes> selected{};
    std::array<float, MaxLocalFogVolumes> selectedDistance2{};
    selectedDistance2.fill(std::numeric_limits<float>::max());
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    for (const SectorCompiledLocalFogVolume& volume : map.compiledLocalFogVolumes) {
        if (!volume.enabled || volume.density <= 0.0f || volume.maxOpacity <= 0.0f) continue;
        const Vector3 toCenter = Vector3Subtract(volume.centerWorld, camera.position);
        const float maxRadius = std::max({volume.radiiWorld.x, volume.radiiWorld.y, volume.radiiWorld.z});
        if (Vector3DotProduct(toCenter, forward) < -maxRadius) continue;
        ++eligibleVolumeCount;
        const float distance2 = Vector3LengthSqr(toCenter);
        int insertAt = std::min(activeVolumeCount, cap - 1);
        if (activeVolumeCount >= cap && distance2 >= selectedDistance2[static_cast<size_t>(cap - 1)]) continue;
        if (activeVolumeCount < cap) ++activeVolumeCount;
        while (insertAt > 0 && (distance2 < selectedDistance2[static_cast<size_t>(insertAt - 1)]
                || (distance2 == selectedDistance2[static_cast<size_t>(insertAt - 1)]
                    && volume.sourceAuthoringFogVolumeId < selected[static_cast<size_t>(insertAt - 1)]->sourceAuthoringFogVolumeId))) {
            selected[static_cast<size_t>(insertAt)] = selected[static_cast<size_t>(insertAt - 1)];
            selectedDistance2[static_cast<size_t>(insertAt)] = selectedDistance2[static_cast<size_t>(insertAt - 1)];
            --insertAt;
        }
        selected[static_cast<size_t>(insertAt)] = &volume;
        selectedDistance2[static_cast<size_t>(insertAt)] = distance2;
    }
    if (activeVolumeCount == 0) return false;

    std::array<float, MaxLocalFogVolumes * 3> centers{};
    std::array<float, MaxLocalFogVolumes * 3> radii{};
    std::array<float, MaxLocalFogVolumes * 3> colors{};
    std::array<float, MaxLocalFogVolumes * 4> paramsA{};
    std::array<float, MaxLocalFogVolumes * 4> paramsB{};
    std::array<float, MaxLocalFogVolumes * 4> lightingA{};
    std::array<float, MaxLocalFogVolumes * 4> lightingB{};
    std::array<float, MaxLocalFogVolumes * 4> lightingC{};
    for (int i = 0; i < activeVolumeCount; ++i) {
        const SectorCompiledLocalFogVolume& volume = *selected[static_cast<size_t>(i)];
        centers[static_cast<size_t>(i * 3 + 0)] = volume.centerWorld.x;
        centers[static_cast<size_t>(i * 3 + 1)] = volume.centerWorld.y;
        centers[static_cast<size_t>(i * 3 + 2)] = volume.centerWorld.z;
        radii[static_cast<size_t>(i * 3 + 0)] = volume.radiiWorld.x;
        radii[static_cast<size_t>(i * 3 + 1)] = volume.radiiWorld.y;
        radii[static_cast<size_t>(i * 3 + 2)] = volume.radiiWorld.z;
        const Vector3 linearColor = engine::SrgbColorBytesToLinearSceneRgb(
                volume.color);
        colors[static_cast<size_t>(i * 3 + 0)] = linearColor.x;
        colors[static_cast<size_t>(i * 3 + 1)] = linearColor.y;
        colors[static_cast<size_t>(i * 3 + 2)] = linearColor.z;
        paramsA[static_cast<size_t>(i * 4 + 0)] = volume.density;
        paramsA[static_cast<size_t>(i * 4 + 1)] = volume.maxOpacity;
        paramsA[static_cast<size_t>(i * 4 + 2)] = volume.edgeSoftness;
        paramsA[static_cast<size_t>(i * 4 + 3)] = volume.noiseScaleWorld;
        paramsB[static_cast<size_t>(i * 4 + 0)] = volume.noiseAmount;
        paramsB[static_cast<size_t>(i * 4 + 1)] = volume.flowDirectionDegrees * DEG2RAD;
        paramsB[static_cast<size_t>(i * 4 + 2)] = volume.flowSpeedWorld;

        const SectorLocalFogStaticLightingSamples& lighting =
                StaticLightingForVolume(map, objectLightProbes, volume);
        const Vector3& negativeXNegativeZ = lighting.corners[0];
        const Vector3& positiveXNegativeZ = lighting.corners[1];
        const Vector3& negativeXPositiveZ = lighting.corners[2];
        const Vector3& positiveXPositiveZ = lighting.corners[3];
        lightingA[static_cast<size_t>(i * 4 + 0)] = negativeXNegativeZ.x;
        lightingA[static_cast<size_t>(i * 4 + 1)] = negativeXNegativeZ.y;
        lightingA[static_cast<size_t>(i * 4 + 2)] = negativeXNegativeZ.z;
        lightingA[static_cast<size_t>(i * 4 + 3)] = positiveXNegativeZ.x;
        lightingB[static_cast<size_t>(i * 4 + 0)] = positiveXNegativeZ.y;
        lightingB[static_cast<size_t>(i * 4 + 1)] = positiveXNegativeZ.z;
        lightingB[static_cast<size_t>(i * 4 + 2)] = negativeXPositiveZ.x;
        lightingB[static_cast<size_t>(i * 4 + 3)] = negativeXPositiveZ.y;
        lightingC[static_cast<size_t>(i * 4 + 0)] = negativeXPositiveZ.z;
        lightingC[static_cast<size_t>(i * 4 + 1)] = positiveXPositiveZ.x;
        lightingC[static_cast<size_t>(i * 4 + 2)] = positiveXPositiveZ.y;
        lightingC[static_cast<size_t>(i * 4 + 3)] = positiveXPositiveZ.z;
    }

    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 correctedUp = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float tanHalfFov = std::tan(camera.fovy * DEG2RAD * 0.5f);
    const float aspect = static_cast<float>(sceneTarget.texture.width) / sceneTarget.texture.height;
    const Vector3 pathLimitSettings{
            LocalFogPathLimitSettings.minimumPathWorld,
            LocalFogPathLimitSettings.heightMultiplier,
            LocalFogPathLimitSettings.saturationPower};
    const float probeFootprintFraction = SectorLocalFogProbeFootprintFraction;

    // Entering the target clears auxiliary sampler bindings, so pass textures
    // must be bound afterward. Flush explicitly before framebuffer/state changes.
    rlDrawRenderBatchActive();
    BeginTextureMode(fogTarget.native);
    ClearBackground(BLANK);
    BeginShaderMode(accumulateShader);
    SetShaderValueTexture(accumulateShader, accumulateLocations.sceneDepth, sceneTarget.depth);
    if (accumulateLocations.shadowMap0 >= 0
            && dynamicLightContext.shadowMaps.shadowMap0 != nullptr
            && dynamicLightContext.shadowMaps.shadowMap0->id != 0) {
        SetShaderValueTexture(
                accumulateShader,
                accumulateLocations.shadowMap0,
                *dynamicLightContext.shadowMaps.shadowMap0);
    }
    if (accumulateLocations.shadowMap1 >= 0
            && dynamicLightContext.shadowMaps.shadowMap1 != nullptr
            && dynamicLightContext.shadowMaps.shadowMap1->id != 0) {
        SetShaderValueTexture(
                accumulateShader,
                accumulateLocations.shadowMap1,
                *dynamicLightContext.shadowMaps.shadowMap1);
    }
    SetShaderValue(accumulateShader, accumulateLocations.volumeCount, &activeVolumeCount, SHADER_UNIFORM_INT);
    SetShaderValue(accumulateShader, accumulateLocations.marchSteps, &steps, SHADER_UNIFORM_INT);
    SetShaderValue(accumulateShader, accumulateLocations.cameraPosition, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(accumulateShader, accumulateLocations.cameraForward, &forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(accumulateShader, accumulateLocations.cameraRight, &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(accumulateShader, accumulateLocations.cameraUp, &correctedUp, SHADER_UNIFORM_VEC3);
    SetShaderValue(accumulateShader, accumulateLocations.tanHalfFov, &tanHalfFov, SHADER_UNIFORM_FLOAT);
    SetShaderValue(accumulateShader, accumulateLocations.aspectRatio, &aspect, SHADER_UNIFORM_FLOAT);
    SetShaderValue(accumulateShader, accumulateLocations.nearPlane, &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(accumulateShader, accumulateLocations.farPlane, &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(accumulateShader, accumulateLocations.runtimeSeconds, &runtimeSeconds, SHADER_UNIFORM_FLOAT);
    SetShaderValue(
            accumulateShader,
            accumulateLocations.pathLimitSettings,
            &pathLimitSettings,
            SHADER_UNIFORM_VEC3);
    SetShaderValue(
            accumulateShader,
            accumulateLocations.probeFootprintFraction,
            &probeFootprintFraction,
            SHADER_UNIFORM_FLOAT);
    SetShaderValueV(accumulateShader, accumulateLocations.fogCenters, centers.data(), SHADER_UNIFORM_VEC3, activeVolumeCount);
    SetShaderValueV(accumulateShader, accumulateLocations.fogRadii, radii.data(), SHADER_UNIFORM_VEC3, activeVolumeCount);
    SetShaderValueV(accumulateShader, accumulateLocations.fogColors, colors.data(), SHADER_UNIFORM_VEC3, activeVolumeCount);
    SetShaderValueV(accumulateShader, accumulateLocations.fogParamsA, paramsA.data(), SHADER_UNIFORM_VEC4, activeVolumeCount);
    SetShaderValueV(accumulateShader, accumulateLocations.fogParamsB, paramsB.data(), SHADER_UNIFORM_VEC4, activeVolumeCount);
    SetShaderValueV(accumulateShader, accumulateLocations.fogLightingA, lightingA.data(), SHADER_UNIFORM_VEC4, activeVolumeCount);
    SetShaderValueV(accumulateShader, accumulateLocations.fogLightingB, lightingB.data(), SHADER_UNIFORM_VEC4, activeVolumeCount);
    SetShaderValueV(accumulateShader, accumulateLocations.fogLightingC, lightingC.data(), SHADER_UNIFORM_VEC4, activeVolumeCount);
    UploadSectorRendererDynamicPointLights(
            accumulateShader,
            accumulateLocations.dynamicLights,
            dynamicLightContext);
    UploadSectorRendererDynamicSpotLightShadowUniforms(
            accumulateShader,
            accumulateLocations.dynamicShadows,
            dynamicLightContext.shadowUniforms);
    // Store premultiplied scattering and opacity without framebuffer blending.
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture), DestinationRect(fogTarget.native.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

    const Vector2 fogTexelSize{1.0f / fogTarget.native.texture.width, 1.0f / fogTarget.native.texture.height};
    const int bilateral = quality ==
            SectorVolumetricQuality::Medium ? 1 : 0;
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneScratch);
    ClearBackground(BLANK);
    BeginShaderMode(compositeShader);
    SetShaderValueTexture(compositeShader, compositeLocations.sceneColor, sceneTarget.texture);
    SetShaderValueTexture(compositeShader, compositeLocations.sceneDepth, sceneTarget.depth);
    SetShaderValueTexture(compositeShader, compositeLocations.fogTexture, fogTarget.native.texture);
    SetShaderValue(compositeShader, compositeLocations.fogTexelSize, &fogTexelSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(compositeShader, compositeLocations.bilateralUpsample, &bilateral, SHADER_UNIFORM_INT);
    // The composite shader produces a complete scene pixel, so store it directly.
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture), DestinationRect(sceneScratch.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

    return true;
}

void SectorLocalFogRenderer::Shutdown()
{
    ReleaseTargets();
    if (ShaderReady(accumulateShader)) UnloadShader(accumulateShader);
    if (ShaderReady(compositeShader)) UnloadShader(compositeShader);
    accumulateShader = Shader{};
    compositeShader = Shader{};
    accumulateLocations = AccumulateShaderLocations{};
    compositeLocations = CompositeShaderLocations{};
    ClearStaticLightingCache();
    cachedProbeData = nullptr;
    cachedProbeCount = 0;
    cachedProbeSourceHashValue = 0;
    cachedMapProbeSourceHashValue = 0;
    failedWidth = 0;
    failedHeight = 0;
    failedScale = 0.0f;
    shaderFailed = false;
    accumulationDiagnostic = "not allocated";
}

} // namespace game
