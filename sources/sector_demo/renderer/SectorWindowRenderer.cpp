#include "sector_demo/renderer/SectorWindowRenderer.h"
#include "sector_demo/renderer/SectorReflectionProbePolicy.h"
#include "sector_demo/renderer/SectorShaderSource.h"

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "engine/render/ColorTransfer.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

const char* WindowVs = R"glsl(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec3 fragLocalPosition;
out vec3 fragLocalNormal;
out vec3 fragWorldAxisX;
out vec3 fragWorldAxisY;
out vec3 fragWorldAxisZ;
void main()
{
    fragWorldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragWorldNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    fragLocalPosition = vertexPosition;
    fragLocalNormal = vertexNormal;
    fragWorldAxisX = normalize((matModel * vec4(1.0, 0.0, 0.0, 0.0)).xyz);
    fragWorldAxisY = normalize((matModel * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
    fragWorldAxisZ = normalize((matModel * vec4(0.0, 0.0, 1.0, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

const char* WindowFs = R"glsl(
#version 330
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec3 fragLocalPosition;
in vec3 fragLocalNormal;
in vec3 fragWorldAxisX;
in vec3 fragWorldAxisY;
in vec3 fragWorldAxisZ;

uniform vec3 cameraPosition;
uniform vec3 glassTint;
uniform float glassOpacity;
uniform float glassRoughness;
uniform float glassSurfaceHaze;
uniform float glassImperfectionStrength;
uniform vec3 glassDimensions;
uniform float glassPatternSeed;
uniform float glassIor;
uniform float glassThickness;
#ifndef WINDOW_FLAT_PASS
#define WINDOW_FLAT_PASS 0
#endif
#if WINDOW_FLAT_PASS == 0
uniform int advancedTransmission;
uniform int flatGlassPass;
#else
#define advancedTransmission 0
#define flatGlassPass WINDOW_FLAT_PASS
#endif
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform mat4 matView;
uniform mat4 matProjection;
uniform samplerCube environmentTexture;
uniform int hasEnvironment;
uniform int environmentBoxProjection;
uniform vec3 environmentCapturePosition;
uniform vec3 environmentInfluenceCenter;
uniform vec3 environmentHalfExtents;
uniform float environmentYaw;
uniform float environmentMaxLod;
uniform float environmentIntensity;
uniform float environmentSpecularScale;
uniform int directionalLightEnabled;
uniform vec3 directionalLightDirection;
uniform vec3 directionalLightColor;
uniform float directionalLightIntensity;

uniform int fogEnabled;
uniform vec3 fogColor;
uniform vec3 fogCameraPosition;
uniform float fogStartDistanceWorld;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeightWorld;
uniform float fogHeightFalloff;

out vec4 finalColor;

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.0000001 ? value * inversesqrt(lengthSquared) : fallback;
}

float GlassHash(vec2 coordinate)
{
    vec3 p = fract(vec3(coordinate.xyx) * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float GlassValueNoise(vec2 coordinate)
{
    vec2 cell = floor(coordinate);
    vec2 fraction = fract(coordinate);
    vec2 blend = fraction * fraction * (3.0 - 2.0 * fraction);
    float a = GlassHash(cell);
    float b = GlassHash(cell + vec2(1.0, 0.0));
    float c = GlassHash(cell + vec2(0.0, 1.0));
    float d = GlassHash(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

float GlassSurfacePattern(vec2 panePosition)
{
    vec2 seedOffset = vec2(
            glassPatternSeed * 0.754877666,
            glassPatternSeed * 0.569840296);
    float broad = GlassValueNoise(panePosition * 0.65 + seedOffset);
    float fine = GlassValueNoise(
            panePosition * 5.0 + seedOffset * 1.731);
    return broad * 0.65 + fine * 0.35;
}

// Evaluate the finite-difference samples with shared hashes when they occupy
// the same lattice cell. Crossing a cell boundary uses the original evaluation.
float GlassNoiseFromCell(vec2 fraction, vec4 hashes)
{
    vec2 blend = fraction * fraction * (3.0 - 2.0 * fraction);
    return mix(mix(hashes.x, hashes.y, blend.x), mix(hashes.z, hashes.w, blend.x), blend.y);
}
vec3 GlassNoiseSamples(vec2 center, vec2 offsetX, vec2 offsetY)
{
    vec2 cell = floor(center);
    vec4 hashes = vec4(GlassHash(cell), GlassHash(cell + vec2(1, 0)),
            GlassHash(cell + vec2(0, 1)), GlassHash(cell + vec2(1, 1)));
    float x = all(equal(floor(offsetX), cell))
            ? GlassNoiseFromCell(fract(offsetX), hashes) : GlassValueNoise(offsetX);
    float y = all(equal(floor(offsetY), cell))
            ? GlassNoiseFromCell(fract(offsetY), hashes) : GlassValueNoise(offsetY);
    return vec3(GlassNoiseFromCell(fract(center), hashes), x, y);
}
vec3 GlassPatternSamples(vec2 position, float stepSize)
{
    vec2 seed = vec2(glassPatternSeed * 0.754877666, glassPatternSeed * 0.569840296);
    vec2 x = position + vec2(stepSize, 0);
    vec2 y = position + vec2(0, stepSize);
    return GlassNoiseSamples(position * 0.65 + seed, x * 0.65 + seed, y * 0.65 + seed) * 0.65
            + GlassNoiseSamples(position * 5.0 + seed * 1.731,
                    x * 5.0 + seed * 1.731, y * 5.0 + seed * 1.731) * 0.35;
}

void GlassPaneCoordinates(out vec2 panePosition,
        out vec3 tangent, out vec3 bitangent)
{
    vec3 localNormal = abs(fragLocalNormal);
    if (localNormal.z >= localNormal.x && localNormal.z >= localNormal.y) {
        panePosition = fragLocalPosition.xy * glassDimensions.xy;
        tangent = fragWorldAxisX;
        bitangent = fragWorldAxisY;
    } else if (localNormal.x >= localNormal.y) {
        panePosition = vec2(
                fragLocalPosition.z * glassDimensions.z,
                fragLocalPosition.y * glassDimensions.y);
        tangent = fragWorldAxisZ;
        bitangent = fragWorldAxisY;
    } else {
        panePosition = vec2(
                fragLocalPosition.x * glassDimensions.x,
                fragLocalPosition.z * glassDimensions.z);
        tangent = fragWorldAxisX;
        bitangent = fragWorldAxisZ;
    }
}

void GlassSurfaceDetail(vec3 geometricNormal,
        out vec3 shadingNormal, out float hazeVariation)
{
    if (glassImperfectionStrength <= 0.0) {
        shadingNormal = geometricNormal;
        hazeVariation = 1.0;
        return;
    }
    vec2 panePosition;
    vec3 tangent;
    vec3 bitangent;
    GlassPaneCoordinates(panePosition, tangent, bitangent);
    float strength = clamp(glassImperfectionStrength, 0.0, 1.0);
    const float derivativeStep = 0.025;
    vec3 samples = GlassPatternSamples(panePosition, derivativeStep);
    float center = samples.x;
    vec2 gradient = (samples.yz - vec2(center)) / derivativeStep;
    float gradientLength = length(gradient);
    if (gradientLength > 1.0) gradient /= gradientLength;
    // tan(4 degrees) bounds the maximum authored normal perturbation.
    float maximumSlope = 0.069926812 * strength;
    shadingNormal = SafeNormalize(geometricNormal
            + tangent * gradient.x * maximumSlope
            + bitangent * gradient.y * maximumSlope, geometricNormal);
    // At the default strength, this varies haze by no more than +/-12%.
    hazeVariation = clamp(1.0 + (center - 0.5) * 1.6 * strength,
            0.2, 1.8);
}

vec3 RotateEnvironment(vec3 direction, float radians)
{
    float c = cos(radians);
    float s = sin(radians);
    return vec3(c * direction.x - s * direction.z,
                direction.y,
                s * direction.x + c * direction.z);
}

float DistributionGgx(vec3 normal, vec3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(normal, halfway), 0.0);
    float denominator = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denominator * denominator, 0.000001);
}

float GeometrySchlickGgx(float ndot, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return ndot / max(ndot * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 normal, vec3 viewDirection,
        vec3 lightDirection, float roughness)
{
    return GeometrySchlickGgx(
            max(dot(normal, viewDirection), 0.0), roughness)
            * GeometrySchlickGgx(
                    max(dot(normal, lightDirection), 0.0), roughness);
}

float FresnelSchlick(float cosTheta, float f0)
{
    return f0 + (1.0 - f0)
            * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 BoxProjectedEnvironmentDirection(vec3 direction)
{
    if (environmentBoxProjection == 0) {
        return RotateEnvironment(direction, -environmentYaw);
    }
    float c = cos(-environmentYaw);
    float s = sin(-environmentYaw);
    vec3 origin = fragWorldPosition - environmentInfluenceCenter;
    vec3 localOrigin = vec3(origin.x*c-origin.z*s, origin.y,
            origin.x*s+origin.z*c);
    vec3 localDirection = vec3(direction.x*c-direction.z*s, direction.y,
            direction.x*s+direction.z*c);
    vec3 safeDirection = mix(vec3(-1.0), vec3(1.0),
            step(vec3(0.0), localDirection))
            * max(abs(localDirection), vec3(0.00001));
    vec3 exitPlane = mix(-environmentHalfExtents, environmentHalfExtents,
            step(vec3(0.0), localDirection));
    vec3 exitDistance = (exitPlane - localOrigin) / safeDirection;
    float distanceToBox = min(exitDistance.x,
            min(exitDistance.y, exitDistance.z));
    vec3 localHit = localOrigin + localDirection * max(distanceToBox, 0.0);
    vec3 captureOffset = environmentCapturePosition
            - environmentInfluenceCenter;
    vec3 localCapture = vec3(captureOffset.x*c-captureOffset.z*s,
            captureOffset.y, captureOffset.x*s+captureOffset.z*c);
    vec3 localLookup = localHit - localCapture;
    c = cos(environmentYaw);
    s = sin(environmentYaw);
    return SafeNormalize(vec3(localLookup.x*c-localLookup.z*s,
            localLookup.y, localLookup.x*s+localLookup.z*c), direction);
}

#if WINDOW_FLAT_PASS == 0
vec3 SampleTransmission(vec3 normal, vec3 viewDirection, float roughness,
        float ior, out float opticalPath)
{
    vec2 baseUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec3 incident = -viewDirection;
    vec3 facingNormal = dot(incident, normal) < 0.0 ? normal : -normal;
    vec3 transmittedDirection = refract(incident, facingNormal, 1.0 / ior);
    transmittedDirection = SafeNormalize(
            transmittedDirection, incident);
    opticalPath = max(glassThickness, 0.001)
            / max(abs(dot(transmittedDirection, facingNormal)), 0.08);
    vec3 exitPosition = fragWorldPosition
            + transmittedDirection * opticalPath;
    vec4 exitClip = matProjection * matView * vec4(exitPosition, 1.0);
    vec2 refractedUv = exitClip.w > 0.00001
            ? exitClip.xy / exitClip.w * 0.5 + 0.5 : baseUv;
    refractedUv = clamp(refractedUv, vec2(0.001), vec2(0.999));
    float roughRadiusPixels = roughness * roughness
            * mix(1.0, 18.0, clamp(opticalPath / 0.08, 0.0, 1.0));
    vec2 radiusUv = vec2(roughRadiusPixels) / max(viewportSize, vec2(1.0));
    const vec2 taps[5] = vec2[5](vec2(0.0), vec2(1.0, 0.0),
            vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0));
    vec3 result = vec3(0.0);
    for (int i = 0; i < 5; ++i) {
        vec2 sampleUv = clamp(refractedUv + taps[i] * radiusUv,
                vec2(0.001), vec2(0.999));
        float sampledDepth = texture(sceneDepth, sampleUv).r;
        if (sampledDepth + 0.0005 < gl_FragCoord.z) sampleUv = baseUv;
        result += texture(sceneColor, sampleUv).rgb;
    }
    return result * 0.2;
}
#endif

vec3 DirectionalSpecular(vec3 normal, vec3 viewDirection, float roughness)
{
    if (directionalLightEnabled == 0 || directionalLightIntensity <= 0.0) {
        return vec3(0.0);
    }
    vec3 lightDirection = SafeNormalize(
            directionalLightDirection, vec3(0.0, 1.0, 0.0));
    float ndotl = dot(normal, lightDirection);
    float ndotv = dot(normal, viewDirection);
    // Do not mirror the sun/moon highlight onto the unlit interior face.
    if (ndotl <= 0.0 || ndotv <= 0.0) {
        return vec3(0.0);
    }
    vec3 halfway = SafeNormalize(viewDirection + lightDirection, normal);
    float distribution = DistributionGgx(normal, halfway, roughness);
    float geometry = GeometrySmith(
            normal, viewDirection, lightDirection, roughness);
    float fresnel = FresnelSchlick(
            max(dot(halfway, viewDirection), 0.0), 0.04);
    float specular = distribution * geometry * fresnel
            / max(4.0 * ndotv * ndotl, 0.001);
    return directionalLightColor * directionalLightIntensity
            * specular * ndotl;
}

float GlassFogAmount()
{
    if (fogEnabled == 0 || fogDensity <= 0.0 || fogMaxOpacity <= 0.0) {
        return 0.0;
    }
    float distanceValue = max(length(fragWorldPosition - fogCameraPosition)
            - fogStartDistanceWorld, 0.0);
    float midpointHeight = (fogCameraPosition.y + fragWorldPosition.y) * 0.5;
    float heightMultiplier = exp(-max(midpointHeight
            - fogReferenceHeightWorld, 0.0) * fogHeightFalloff);
    return min(1.0 - exp(-fogDensity * distanceValue
            * heightMultiplier), fogMaxOpacity);
}

void main()
{
    vec3 normal = SafeNormalize(fragWorldNormal, vec3(0.0, 1.0, 0.0));
    vec3 viewDirection = SafeNormalize(cameraPosition - fragWorldPosition, normal);
#if WINDOW_FLAT_PASS == 0
    if (advancedTransmission != 0) {
        vec2 baseUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
        float opaqueDepth = texture(sceneDepth, baseUv).r;
        if (gl_FragCoord.z > opaqueDepth + 0.00001) discard;
    }
#endif
    float roughness = clamp(glassRoughness, 0.045, 1.0);
    float ior = clamp(glassIor, 1.0, 2.5);
    float ndotv = clamp(dot(normal, viewDirection), 0.0, 1.0);
    float fresnel = clamp(0.04 + 0.96
            * pow(clamp(1.0 - ndotv, 0.0, 1.0), 5.0), 0.0, 1.0);

    float opacity = clamp(glassOpacity, 0.0, 1.0);
    float blocker = clamp(mix(opacity, 1.0, fresnel), 0.0, 1.0);
    vec3 tintFilter = mix(vec3(1.0), clamp(glassTint, 0.0, 1.0), opacity);
    vec3 transmission = clamp((1.0 - blocker) * tintFilter, 0.0, 1.0);

    if (advancedTransmission == 0 && flatGlassPass == 1) {
        // With ZERO/SRC_COLOR blending, this filters the existing opaque scene
        // and all pre-glass light effects without adding any tint radiance.
        finalColor = vec4(transmission, 1.0);
        return;
    }

    vec3 shadingNormal;
    float hazeVariation;
    GlassSurfaceDetail(normal, shadingNormal, hazeVariation);
    float shadingNdotV = clamp(dot(shadingNormal, viewDirection), 0.0, 1.0);
    float shadingFresnel = clamp(0.04 + 0.96
            * pow(clamp(1.0 - shadingNdotV, 0.0, 1.0), 5.0), 0.0, 1.0);
    float hazeWeight = min(opacity * clamp(glassSurfaceHaze, 0.0, 1.0)
            * (1.0 - fresnel) * hazeVariation, 0.20);
    vec3 haze = clamp(glassTint, 0.0, 1.0) * hazeWeight;

    vec3 reflection = vec3(0);
    if (environmentSpecularScale > 0.0) {
        reflection = SampleSectorEnvironment(fragWorldPosition,
                reflect(-viewDirection,shadingNormal),roughness)*environmentSpecularScale;
    }
    vec3 direct = DirectionalSpecular(
            shadingNormal, viewDirection, roughness);
    vec3 rgb;
#if WINDOW_FLAT_PASS == 0
    if (advancedTransmission != 0) {
        float opticalPath = 0.0;
        vec3 sceneTransmission = SampleTransmission(
                shadingNormal, viewDirection, roughness, ior, opticalPath);
        float normalizedPath = clamp(opticalPath / 0.04, 0.0, 32.0);
        vec3 absorption = pow(max(glassTint, vec3(0.015)),
                vec3(normalizedPath * mix(0.12, 0.65, opacity)));
        float neutralTransmission = exp(-opacity * normalizedPath);
        rgb = sceneTransmission * absorption
                        * neutralTransmission * (1.0 - shadingFresnel)
                + reflection * shadingFresnel + direct + haze;
        float fogAmount = GlassFogAmount();
        rgb = mix(rgb, fogColor, fogAmount);
        finalColor = vec4(clamp(rgb, vec3(0.0), vec3(65504.0)), 1.0);
        return;
    } else
#endif
    {
        rgb = reflection * shadingFresnel
                + direct + haze;
        float fogAmount = GlassFogAmount();
        rgb = mix(rgb, fogColor * blocker, fogAmount);
    }
    rgb = clamp(rgb, vec3(0.0), vec3(65504.0));
    // The additive flat-glass pass preserves the destination alpha.
    finalColor = vec4(rgb, 0.0);
}
)glsl";

} // namespace

bool SectorWindowRenderer::Initialize(std::size_t capacity)
{
    Shutdown();
    Reserve(capacity);
    for (int variant = 0; variant < 3; ++variant) {
        active = {};
        const std::string reflectionSource = InsertSectorShaderPreamble(
            AddSectorReflectionShaderSource(WindowFs),
            "#define WINDOW_FLAT_PASS " + std::to_string(variant) + "\n");
        active.shader = LoadShaderFromMemory(WindowVs, reflectionSource.c_str());
        active.reflectionLocations = LoadSectorReflectionShaderLocations(active.shader);
        if (active.shader.id == 0) {
            Shutdown();
            return false;
        }
        active.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(active.shader, "vertexPosition");
        active.shader.locs[SHADER_LOC_VERTEX_NORMAL] =
            GetShaderLocationAttrib(active.shader, "vertexNormal");
        active.shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(active.shader, "mvp");
        active.shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(active.shader, "matModel");
        active.shader.locs[SHADER_LOC_MATRIX_NORMAL] =
            GetShaderLocation(active.shader, "matNormal");
        active.shader.locs[SHADER_LOC_MAP_CUBEMAP] =
            GetShaderLocation(active.shader, "environmentTexture");
        active.cameraPositionLoc = GetShaderLocation(active.shader, "cameraPosition");
        active.tintLoc = GetShaderLocation(active.shader, "glassTint");
        active.opacityLoc = GetShaderLocation(active.shader, "glassOpacity");
        active.roughnessLoc = GetShaderLocation(active.shader, "glassRoughness");
        active.surfaceHazeLoc = GetShaderLocation(active.shader, "glassSurfaceHaze");
        active.imperfectionStrengthLoc =
            GetShaderLocation(active.shader, "glassImperfectionStrength");
        active.dimensionsLoc = GetShaderLocation(active.shader, "glassDimensions");
        active.patternSeedLoc = GetShaderLocation(active.shader, "glassPatternSeed");
        active.iorLoc = GetShaderLocation(active.shader, "glassIor");
        active.thicknessLoc = GetShaderLocation(active.shader, "glassThickness");
        active.advancedTransmissionLoc = GetShaderLocation(active.shader, "advancedTransmission");
        active.flatGlassPassLoc = GetShaderLocation(active.shader, "flatGlassPass");
        active.sceneColorLoc = GetShaderLocation(active.shader, "sceneColor");
        active.sceneDepthLoc = GetShaderLocation(active.shader, "sceneDepth");
        active.shader.locs[SHADER_LOC_MAP_DIFFUSE] = active.sceneColorLoc;
        active.shader.locs[SHADER_LOC_MAP_SPECULAR] = active.sceneDepthLoc;
        active.viewportSizeLoc = GetShaderLocation(active.shader, "viewportSize");
        active.viewMatrixLoc = GetShaderLocation(active.shader, "matView");
        active.projectionMatrixLoc = GetShaderLocation(active.shader, "matProjection");
        active.hasEnvironmentLoc = GetShaderLocation(active.shader, "hasEnvironment");
        active.environmentBoxProjectionLoc =
            GetShaderLocation(active.shader, "environmentBoxProjection");
        active.environmentCapturePositionLoc =
            GetShaderLocation(active.shader, "environmentCapturePosition");
        active.environmentInfluenceCenterLoc =
            GetShaderLocation(active.shader, "environmentInfluenceCenter");
        active.environmentHalfExtentsLoc =
            GetShaderLocation(active.shader, "environmentHalfExtents");
        active.environmentYawLoc = GetShaderLocation(active.shader, "environmentYaw");
        active.environmentMaxLodLoc = GetShaderLocation(active.shader, "environmentMaxLod");
        active.environmentIntensityLoc = GetShaderLocation(active.shader, "environmentIntensity");
        active.environmentSpecularScaleLoc =
            GetShaderLocation(active.shader, "environmentSpecularScale");
        active.directionalLightEnabledLoc =
            GetShaderLocation(active.shader, "directionalLightEnabled");
        active.directionalLightDirectionLoc =
            GetShaderLocation(active.shader, "directionalLightDirection");
        active.directionalLightColorLoc = GetShaderLocation(active.shader, "directionalLightColor");
        active.directionalLightIntensityLoc =
            GetShaderLocation(active.shader, "directionalLightIntensity");
        active.fogLocations = GetSectorFogShaderLocations(active.shader);

        active.material = LoadMaterialDefault();
        active.material.shader = active.shader;
        variants[variant] = active;
    }
    materialLoaded = true;
    cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    meshLoaded = cube.vertexCount > 0;
    return meshLoaded;
}

void SectorWindowRenderer::Shutdown()
{
    drawItems.clear();
    if (meshLoaded)
        UnloadMesh(cube);
    cube = {};
    meshLoaded = false;
    for (auto &resources : variants) {
        if (resources.material.maps) {
            resources.material.maps[MATERIAL_MAP_CUBEMAP].texture = {};
            UnloadMaterial(resources.material);
        } else if (resources.shader.id)
            UnloadShader(resources.shader);
        resources = {};
    }
    active = {};
    materialLoaded = false;
    consideredCount = 0;
    drawnCount = 0;
    localEnvironmentCount = 0;
    globalEnvironmentCount = 0;
    missingEnvironmentCount = 0;
}

void SectorWindowRenderer::Reserve(std::size_t capacity)
{
    drawItems.reserve(capacity);
}

bool SectorWindowRenderer::PrepareVisibleWindows(engine::World &world, const Camera3D &camera,
                                                 float aspect,
                                                 const RuntimePortalVisibilityResult *visibility,
                                                 bool enabled)
{
    consideredCount = drawnCount = 0;
    drawItems.clear();
    world.ForEach<SectorObjectTransform, SectorObject, SectorWindow>(
        [&](engine::Entity entity, SectorObjectTransform &transform, SectorObject &object,
            SectorWindow &window) {
            ++consideredCount;
            if (!enabled)
                return;
            const BoundingBox bounds =
                TransformSectorDoorModelBounds({{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
                                               BuildSectorWindowModelMatrix(transform, window));
            if (!SectorPaneVisible(object.visible, window.visible, window.frontSectorId,
                                   window.backSectorId, visibility, camera, aspect,
                                   rlGetCullDistanceNear(), rlGetCullDistanceFar(), bounds))
                return;
            if (drawItems.size() == drawItems.capacity() && !capacityWarned) {
                TraceLog(LOG_WARNING, "RENDER: window draw capacity exceeded; frame allocation");
                capacityWarned = true;
            }
            drawItems.push_back({entity, window.placedObjectId,
                                 Vector3DistanceSqr(transform.position, camera.position)});
        });
    std::sort(drawItems.begin(), drawItems.end(), [](const DrawItem &a, const DrawItem &b) {
        if (a.distanceSquared != b.distanceSquared) {
            return a.distanceSquared > b.distanceSquared;
        }
        return a.placedObjectId < b.placedObjectId;
    });
    return !drawItems.empty();
}

void SectorWindowRenderer::Draw(const SectorWindowDrawContext &context)
{
    drawnCount = localEnvironmentCount = globalEnvironmentCount = missingEnvironmentCount = 0;
    if (!materialLoaded || !meshLoaded || context.assets == nullptr || context.world == nullptr)
        return;

    const int advancedTransmission =
        context.advancedTransmission && context.sceneColor && context.sceneDepth ? 1 : 0;
    const auto pbr = NormalizeSectorPbrContributionSettings(context.pbr);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    if (advancedTransmission != 0) {
        rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY);
    }
    if (advancedTransmission != 0)
        rlDisableDepthTest();
    else
        rlEnableDepthTest();
    rlDisableDepthMask();
    rlEnableBackfaceCulling();

    for (const DrawItem &item : drawItems) {
        if (!context.world->IsAlive(item.entity) ||
            !context.world->Has<SectorObjectTransform>(item.entity) ||
            !context.world->Has<SectorObject>(item.entity) ||
            !context.world->Has<SectorWindow>(item.entity))
            continue;
        const SectorObjectTransform &transform =
            context.world->Get<SectorObjectTransform>(item.entity);
        const SectorObject &object = context.world->Get<SectorObject>(item.entity);
        const SectorWindow &window = context.world->Get<SectorWindow>(item.entity);

        const int firstPass = advancedTransmission ? 0 : 1;
        const int lastPass = advancedTransmission ? 0 : 2;
        for (int pass = firstPass; pass <= lastPass; ++pass) {
            active = variants[pass];
            if (context.profiler)
                context.profiler->Begin(pass == 1 ? SectorWorldStage::GlassTransmission
                                                  : SectorWorldStage::GlassReflection);
            if (active.cameraPositionLoc >= 0)
                SetShaderValue(active.shader, active.cameraPositionLoc, &context.camera.position,
                               SHADER_UNIFORM_VEC3);
            const Texture2D originalDiffuse = active.material.maps[MATERIAL_MAP_DIFFUSE].texture;
            const Texture2D originalSpecular = active.material.maps[MATERIAL_MAP_SPECULAR].texture;
            if (advancedTransmission != 0) {
                active.material.maps[MATERIAL_MAP_DIFFUSE].texture = *context.sceneColor;
                active.material.maps[MATERIAL_MAP_SPECULAR].texture = *context.sceneDepth;
            }
            if (active.advancedTransmissionLoc >= 0)
                SetShaderValue(active.shader, active.advancedTransmissionLoc, &advancedTransmission,
                               SHADER_UNIFORM_INT);
            if (active.viewportSizeLoc >= 0)
                SetShaderValue(active.shader, active.viewportSizeLoc, &context.viewportSize,
                               SHADER_UNIFORM_VEC2);
            const Matrix viewMatrix = GetCameraMatrix(context.camera);
            const Matrix projectionMatrix = rlGetMatrixProjection();
            if (active.viewMatrixLoc >= 0)
                SetShaderValueMatrix(active.shader, active.viewMatrixLoc, viewMatrix);
            if (active.projectionMatrixLoc >= 0)
                SetShaderValueMatrix(active.shader, active.projectionMatrixLoc, projectionMatrix);
            if (active.environmentSpecularScaleLoc >= 0)
                SetShaderValue(active.shader, active.environmentSpecularScaleLoc,
                               &pbr.worldEnvironmentSpecularScale, SHADER_UNIFORM_FLOAT);
            const SectorTopologyDirectionalLightSettings directionalLight =
                NormalizeSectorTopologyDirectionalLightSettings(context.directionalLight);
            const int directionalLightEnabled = directionalLight.enabled ? 1 : 0;
            const Vector3 directionalLightColor =
                engine::SrgbColorBytesToLinearSceneRgb(directionalLight.color);
            if (active.directionalLightEnabledLoc >= 0)
                SetShaderValue(active.shader, active.directionalLightEnabledLoc,
                               &directionalLightEnabled, SHADER_UNIFORM_INT);
            if (active.directionalLightDirectionLoc >= 0)
                SetShaderValue(active.shader, active.directionalLightDirectionLoc,
                               &directionalLight.directionToLight, SHADER_UNIFORM_VEC3);
            if (active.directionalLightColorLoc >= 0)
                SetShaderValue(active.shader, active.directionalLightColorLoc,
                               &directionalLightColor, SHADER_UNIFORM_VEC3);
            if (active.directionalLightIntensityLoc >= 0)
                SetShaderValue(active.shader, active.directionalLightIntensityLoc,
                               &directionalLight.intensity, SHADER_UNIFORM_FLOAT);
            UploadSectorFogShaderValues(active.shader, active.fogLocations, context.fog);

            const Vector3 tint = engine::SrgbColorBytesToLinearSceneRgb(window.tint);
            if (active.tintLoc >= 0)
                SetShaderValue(active.shader, active.tintLoc, &tint, SHADER_UNIFORM_VEC3);
            if (active.opacityLoc >= 0)
                SetShaderValue(active.shader, active.opacityLoc, &window.opacity,
                               SHADER_UNIFORM_FLOAT);
            if (active.roughnessLoc >= 0)
                SetShaderValue(active.shader, active.roughnessLoc, &window.roughness,
                               SHADER_UNIFORM_FLOAT);
            if (active.surfaceHazeLoc >= 0)
                SetShaderValue(active.shader, active.surfaceHazeLoc, &window.surfaceHaze,
                               SHADER_UNIFORM_FLOAT);
            if (active.imperfectionStrengthLoc >= 0)
                SetShaderValue(active.shader, active.imperfectionStrengthLoc,
                               &window.imperfectionStrength, SHADER_UNIFORM_FLOAT);
            const Vector3 glassDimensions{window.width, window.height, window.thickness};
            if (active.dimensionsLoc >= 0)
                SetShaderValue(active.shader, active.dimensionsLoc, &glassDimensions,
                               SHADER_UNIFORM_VEC3);
            const float patternSeed = static_cast<float>(window.placedObjectId);
            if (active.patternSeedLoc >= 0)
                SetShaderValue(active.shader, active.patternSeedLoc, &patternSeed,
                               SHADER_UNIFORM_FLOAT);
            if (active.iorLoc >= 0)
                SetShaderValue(active.shader, active.iorLoc, &window.indexOfRefraction,
                               SHADER_UNIFORM_FLOAT);
            if (active.thicknessLoc >= 0)
                SetShaderValue(active.shader, active.thicknessLoc, &window.thickness,
                               SHADER_UNIFORM_FLOAT);

            if (pass != 1) {
                SectorPbrEnvironmentBlend reflectionBlend;
                if (context.environment) {
                    const Vector3 portalNormal{window.normal.x, 0.0f, window.normal.y};
                    const bool back = Vector3DotProduct(Vector3Subtract(context.camera.position,
                                                                        transform.position),
                                                        portalNormal) > 0;
                    const Vector3 receiver = Vector3Add(
                        transform.position, Vector3Scale(portalNormal, back ? 0.25f : -0.25f));
                    const BoundingBox reflectionBounds = TransformSectorDoorModelBounds(
                        {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
                        BuildSectorWindowModelMatrix(transform, window));
                    reflectionBlend = SelectSectorPbrEnvironmentBlend(
                        *context.environment, receiver,
                        back ? window.backSectorId : window.frontSectorId, true, nullptr,
                        SectorReflectionDemandForBounds(context.environment->demandCollector,
                                                        reflectionBounds));
                }
                UploadSectorReflectionBlend(active.shader, active.reflectionLocations,
                                            reflectionBlend, *context.assets);
                const auto &selection = reflectionBlend.first;
                const TextureCubemap *cubemap = context.assets->GetCubemap(selection.cubemap);
                const int hasEnvironment = cubemap != nullptr && cubemap->id != 0 &&
                                                   pbr.worldEnvironmentSpecularScale > 0.0f
                                               ? 1
                                               : 0;
                active.material.maps[MATERIAL_MAP_CUBEMAP].texture =
                    hasEnvironment != 0 ? *cubemap : Texture2D{};
                if (active.hasEnvironmentLoc >= 0)
                    SetShaderValue(active.shader, active.hasEnvironmentLoc, &hasEnvironment,
                                   SHADER_UNIFORM_INT);
                const int boxProjection = selection.boxProjection ? 1 : 0;
                if (active.environmentBoxProjectionLoc >= 0)
                    SetShaderValue(active.shader, active.environmentBoxProjectionLoc,
                                   &boxProjection, SHADER_UNIFORM_INT);
                if (active.environmentCapturePositionLoc >= 0)
                    SetShaderValue(active.shader, active.environmentCapturePositionLoc,
                                   &selection.capturePosition, SHADER_UNIFORM_VEC3);
                if (active.environmentInfluenceCenterLoc >= 0)
                    SetShaderValue(active.shader, active.environmentInfluenceCenterLoc,
                                   &selection.influenceCenter, SHADER_UNIFORM_VEC3);
                if (active.environmentHalfExtentsLoc >= 0)
                    SetShaderValue(active.shader, active.environmentHalfExtentsLoc,
                                   &selection.halfExtents, SHADER_UNIFORM_VEC3);
                if (active.environmentYawLoc >= 0)
                    SetShaderValue(active.shader, active.environmentYawLoc, &selection.yawRadians,
                                   SHADER_UNIFORM_FLOAT);
                if (active.environmentMaxLodLoc >= 0)
                    SetShaderValue(active.shader, active.environmentMaxLodLoc, &selection.maxLod,
                                   SHADER_UNIFORM_FLOAT);
                const float environmentIntensity =
                    selection.localProbe ? selection.intensity : 0.15f;
                if (active.environmentIntensityLoc >= 0)
                    SetShaderValue(active.shader, active.environmentIntensityLoc,
                                   &environmentIntensity, SHADER_UNIFORM_FLOAT);
                if (hasEnvironment == 0)
                    ++missingEnvironmentCount;
                else if (selection.localProbe)
                    ++localEnvironmentCount;
                else
                    ++globalEnvironmentCount;
            }

            const Matrix modelMatrix = BuildSectorWindowModelMatrix(transform, window);
            if (pass == 1) {
                rlSetBlendFactorsSeparate(RL_ZERO, RL_SRC_COLOR, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                                          RL_FUNC_ADD);
                rlSetBlendMode(BLEND_CUSTOM_SEPARATE);
            } else if (pass == 2) {
                rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                                          RL_FUNC_ADD);
                rlSetBlendMode(BLEND_CUSTOM_SEPARATE);
            }
            DrawMesh(cube, active.material, modelMatrix);
            if (context.profiler)
                context.profiler->End();
            active.material.maps[MATERIAL_MAP_CUBEMAP].texture = {};
            active.material.maps[MATERIAL_MAP_DIFFUSE].texture = originalDiffuse;
            active.material.maps[MATERIAL_MAP_SPECULAR].texture = originalSpecular;
        }
        ++drawnCount;
    }

    rlDrawRenderBatchActive();
    active.material.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    if (context.renderDebugText != nullptr) {
        *context.renderDebugText +=
            " | windows: " + std::to_string(drawnCount) + " drawn / " +
            std::to_string(consideredCount) + " considered; env local/global/none " +
            std::to_string(localEnvironmentCount) + "/" + std::to_string(globalEnvironmentCount) +
            "/" + std::to_string(missingEnvironmentCount) +
            (advancedTransmission != 0 ? "; advanced" : "; flat two-pass");
    }
}

} // namespace game
