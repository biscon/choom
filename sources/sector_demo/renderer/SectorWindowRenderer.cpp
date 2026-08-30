#include "sector_demo/renderer/SectorWindowRenderer.h"

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
void main()
{
    fragWorldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragWorldNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

const char* WindowFs = R"glsl(
#version 330
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

uniform vec3 cameraPosition;
uniform vec3 glassTint;
uniform float glassOpacity;
uniform float glassRoughness;
uniform float glassIor;
uniform float glassThickness;
uniform vec3 glassAmbient;
uniform int advancedTransmission;
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

#define MAX_DYNAMIC_LIGHTS 32
uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightTypes[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightDirections[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];

#define MAX_STATIC_SPECULAR_LIGHTS 4
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

vec3 RotateEnvironment(vec3 direction, float radians)
{
    float c = cos(radians);
    float s = sin(radians);
    return vec3(c * direction.x - s * direction.z,
                direction.y,
                s * direction.x + c * direction.z);
}

float LightCone(int type, vec3 directionFromLight, vec3 lightDirection,
        float innerCos, float outerCos)
{
    if (type != 1) return 1.0;
    float cone = dot(SafeNormalize(lightDirection, vec3(0.0, -1.0, 0.0)),
            SafeNormalize(directionFromLight, vec3(0.0, -1.0, 0.0)));
    return abs(innerCos - outerCos) > 0.0001
            ? smoothstep(outerCos, innerCos, cone)
            : step(innerCos, cone);
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

vec3 SpecularLight(vec3 position, vec3 color, float radius, float intensity,
        int type, vec3 direction, float innerCos, float outerCos,
        vec3 normal, vec3 viewDirection, float roughness, float f0)
{
    vec3 toLight = position - fragWorldPosition;
    float distanceToLight = length(toLight);
    if (radius <= 0.0 || distanceToLight >= radius || intensity <= 0.0) {
        return vec3(0.0);
    }
    vec3 lightDirection = distanceToLight > 0.0001
            ? toLight / distanceToLight : normal;
    float ndotl = max(dot(normal, lightDirection), 0.0);
    float ndotv = max(dot(normal, viewDirection), 0.0);
    float attenuation = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
    attenuation *= attenuation;
    float coneAttenuation = LightCone(
            type, -lightDirection, direction, innerCos, outerCos);
    if (ndotl <= 0.0 || ndotv <= 0.0 || attenuation <= 0.0
            || coneAttenuation <= 0.0) {
        return vec3(0.0);
    }
    vec3 halfway = SafeNormalize(viewDirection + lightDirection, normal);
    float distribution = DistributionGgx(normal, halfway, roughness);
    float geometry = GeometrySmith(
            normal, viewDirection, lightDirection, roughness);
    float fresnel = FresnelSchlick(
            max(dot(halfway, viewDirection), 0.0), f0);
    float specular = distribution * geometry * fresnel
            / max(4.0 * ndotv * ndotl, 0.001);
    vec3 radiance = color * intensity
            * attenuation * attenuation * coneAttenuation;
    return radiance * specular * ndotl;
}

void main()
{
    vec3 normal = SafeNormalize(fragWorldNormal, vec3(0.0, 1.0, 0.0));
    vec3 viewDirection = SafeNormalize(cameraPosition - fragWorldPosition, normal);
    if (advancedTransmission != 0) {
        vec2 baseUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
        float opaqueDepth = texture(sceneDepth, baseUv).r;
        if (gl_FragCoord.z > opaqueDepth + 0.00001) discard;
    }
    float roughness = clamp(glassRoughness, 0.045, 1.0);
    float ior = clamp(glassIor, 1.0, 2.5);
    float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
    float ndotv = max(dot(normal, viewDirection), 0.0);
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - ndotv, 5.0);

    vec3 reflection = vec3(0.0);
    if (hasEnvironment != 0) {
        vec3 reflected = BoxProjectedEnvironmentDirection(
                reflect(-viewDirection, normal));
        reflection = textureLod(environmentTexture, reflected,
                roughness * max(environmentMaxLod, 0.0)).rgb
                * environmentIntensity * environmentSpecularScale * fresnel;
    }

    vec3 direct = vec3(0.0);
    for (int i = 0; i < dynamicLightCount && i < MAX_DYNAMIC_LIGHTS; ++i) {
        direct += SpecularLight(dynamicLightPositions[i], dynamicLightColors[i],
                dynamicLightRadii[i], dynamicLightIntensities[i],
                dynamicLightTypes[i], dynamicLightDirections[i],
                dynamicLightInnerConeCos[i], dynamicLightOuterConeCos[i],
                normal, viewDirection, roughness, f0);
    }
    for (int i = 0; i < staticSpecularLightCount
            && i < MAX_STATIC_SPECULAR_LIGHTS; ++i) {
        direct += SpecularLight(staticSpecularLightPositions[i],
                staticSpecularLightColors[i], staticSpecularLightRadii[i],
                staticSpecularLightIntensities[i], staticSpecularLightTypes[i],
                staticSpecularLightDirections[i],
                staticSpecularLightInnerConeCos[i],
                staticSpecularLightOuterConeCos[i], normal, viewDirection,
                roughness, f0);
    }

    float opacity = clamp(glassOpacity, 0.0, 1.0);
    float alpha = clamp(opacity + (1.0 - opacity) * fresnel, 0.0, 1.0);
    vec3 rgb;
    if (advancedTransmission != 0) {
        float opticalPath = 0.0;
        vec3 sceneTransmission = SampleTransmission(
                normal, viewDirection, roughness, ior, opticalPath);
        float normalizedPath = clamp(opticalPath / 0.04, 0.0, 32.0);
        vec3 absorption = pow(max(glassTint, vec3(0.015)),
                vec3(normalizedPath * mix(0.12, 0.65, opacity)));
        float neutralTransmission = exp(-opacity * normalizedPath);
        rgb = sceneTransmission * absorption
                        * neutralTransmission * (1.0 - fresnel)
                + glassTint * max(glassAmbient, vec3(0.015)) * opacity
                + reflection + direct;
        alpha = 1.0;
    } else {
        rgb = glassTint * max(glassAmbient, vec3(0.015)) * opacity
                + reflection + direct;
    }

    if (fogEnabled != 0 && fogDensity > 0.0 && fogMaxOpacity > 0.0) {
        float distanceValue = max(length(fragWorldPosition - fogCameraPosition)
                - fogStartDistanceWorld, 0.0);
        float midpointHeight = (fogCameraPosition.y + fragWorldPosition.y) * 0.5;
        float heightMultiplier = exp(-max(midpointHeight
                - fogReferenceHeightWorld, 0.0) * fogHeightFalloff);
        float fogAmount = min(1.0 - exp(-fogDensity * distanceValue
                * heightMultiplier), fogMaxOpacity);
        rgb = mix(rgb, fogColor * alpha, fogAmount);
    }
    rgb = clamp(rgb, vec3(0.0), vec3(65504.0));
    finalColor = vec4(rgb, alpha);
}
)glsl";

Vector3 WindowAmbient(const SectorObjectLighting& lighting)
{
    const BakedObjectLightingSample* sample = lighting.baked.valid
            ? &lighting.baked
            : lighting.vertical.lower.valid ? &lighting.vertical.lower
            : lighting.vertical.upper.valid ? &lighting.vertical.upper
            : nullptr;
    if (sample == nullptr) return Vector3{0.15f, 0.15f, 0.15f};
    Vector3 ambient{};
    for (const Vector3 face : sample->ambientCube) {
        ambient = Vector3Add(ambient, face);
    }
    return Vector3Scale(ambient, 1.0f / 6.0f);
}

bool WindowVisible(
        const SectorWindow& window,
        const RuntimePortalVisibilityResult* visibility)
{
    if (visibility == nullptr
            || !visibility->validStartSector
            || visibility->fallbackDrawAll) return true;
    return ShouldDrawRuntimeSectorForVisibility(
                    window.frontSectorId, *visibility)
            || ShouldDrawRuntimeSectorForVisibility(
                    window.backSectorId, *visibility);
}

SectorPbrEnvironmentSelection SelectWindowEnvironment(
        const SectorPbrEnvironment& environment,
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorWindow& window,
        Vector3 cameraPosition,
        bool includeLocalProbes)
{
    const Vector3 portalNormal{window.normal.x, 0.0f, window.normal.y};
    const float cameraSide = Vector3DotProduct(
            Vector3Subtract(cameraPosition, transform.position), portalNormal);
    const float direction = cameraSide > 0.0f ? 1.0f : -1.0f;
    const int viewerSectorId = cameraSide > 0.0f
            ? window.backSectorId : window.frontSectorId;
    const Vector3 receiver = Vector3Add(
            transform.position, Vector3Scale(portalNormal, direction * 0.25f));
    SectorPbrEnvironmentSelection selection = SelectSectorPbrEnvironment(
            environment, receiver, viewerSectorId, includeLocalProbes);
    if (selection.localProbe) return selection;
    const SectorPbrEnvironmentSelection centerSelection =
            SelectSectorPbrEnvironment(
                    environment,
                    transform.position,
                    object.currentSectorId,
                    includeLocalProbes);
    if (centerSelection.localProbe) return centerSelection;

    const SectorPbrEnvironment::LocalProbe* nearestSectorProbe = nullptr;
    float nearestDistanceSquared = 0.0f;
    if (includeLocalProbes) {
        for (const SectorPbrEnvironment::LocalProbe& candidate
                : environment.localProbes) {
            const SectorCompiledReflectionProbe& probe = candidate.definition;
            if (!probe.enabled || engine::IsNull(candidate.cubemap)
                    || probe.topologySectorId != viewerSectorId) continue;
            const float distanceSquared = Vector3DistanceSqr(
                    receiver, probe.influenceCenterWorld);
            if (nearestSectorProbe == nullptr
                    || probe.priority > nearestSectorProbe->definition.priority
                    || (probe.priority
                                    == nearestSectorProbe->definition.priority
                            && (distanceSquared < nearestDistanceSquared
                                    || (distanceSquared == nearestDistanceSquared
                                            && probe.sourceAuthoringProbeId
                                                    < nearestSectorProbe->definition
                                                            .sourceAuthoringProbeId)))) {
                nearestSectorProbe = &candidate;
                nearestDistanceSquared = distanceSquared;
            }
        }
    }
    if (nearestSectorProbe != nullptr) {
        const SectorCompiledReflectionProbe& probe =
                nearestSectorProbe->definition;
        return SectorPbrEnvironmentSelection{
                nearestSectorProbe->cubemap,
                probe.capturePositionWorld,
                probe.influenceCenterWorld,
                probe.halfExtentsWorld,
                probe.yawRadians,
                probe.intensity,
                static_cast<float>(std::max(0, nearestSectorProbe->mipCount - 1)),
                true,
                true};
    }
    return selection;
}

} // namespace

bool SectorWindowRenderer::Initialize(std::size_t capacity)
{
    Shutdown();
    Reserve(capacity);
    shader = LoadShaderFromMemory(WindowVs, WindowFs);
    if (shader.id == 0) return false;
    shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] =
            GetShaderLocationAttrib(shader, "vertexNormal");
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_CUBEMAP] =
            GetShaderLocation(shader, "environmentTexture");
    cameraPositionLoc = GetShaderLocation(shader, "cameraPosition");
    tintLoc = GetShaderLocation(shader, "glassTint");
    opacityLoc = GetShaderLocation(shader, "glassOpacity");
    roughnessLoc = GetShaderLocation(shader, "glassRoughness");
    iorLoc = GetShaderLocation(shader, "glassIor");
    thicknessLoc = GetShaderLocation(shader, "glassThickness");
    ambientLoc = GetShaderLocation(shader, "glassAmbient");
    advancedTransmissionLoc = GetShaderLocation(shader, "advancedTransmission");
    sceneColorLoc = GetShaderLocation(shader, "sceneColor");
    sceneDepthLoc = GetShaderLocation(shader, "sceneDepth");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = sceneColorLoc;
    shader.locs[SHADER_LOC_MAP_SPECULAR] = sceneDepthLoc;
    viewportSizeLoc = GetShaderLocation(shader, "viewportSize");
    viewMatrixLoc = GetShaderLocation(shader, "matView");
    projectionMatrixLoc = GetShaderLocation(shader, "matProjection");
    hasEnvironmentLoc = GetShaderLocation(shader, "hasEnvironment");
    environmentBoxProjectionLoc =
            GetShaderLocation(shader, "environmentBoxProjection");
    environmentCapturePositionLoc =
            GetShaderLocation(shader, "environmentCapturePosition");
    environmentInfluenceCenterLoc =
            GetShaderLocation(shader, "environmentInfluenceCenter");
    environmentHalfExtentsLoc =
            GetShaderLocation(shader, "environmentHalfExtents");
    environmentYawLoc = GetShaderLocation(shader, "environmentYaw");
    environmentMaxLodLoc = GetShaderLocation(shader, "environmentMaxLod");
    environmentIntensityLoc = GetShaderLocation(shader, "environmentIntensity");
    environmentSpecularScaleLoc =
            GetShaderLocation(shader, "environmentSpecularScale");
    dynamicLightLocations.dynamicLightCount =
            GetShaderLocation(shader, "dynamicLightCount");
    dynamicLightLocations.dynamicLightPositions =
            GetShaderLocation(shader, "dynamicLightPositions[0]");
    dynamicLightLocations.dynamicLightColors =
            GetShaderLocation(shader, "dynamicLightColors[0]");
    dynamicLightLocations.dynamicLightRadii =
            GetShaderLocation(shader, "dynamicLightRadii[0]");
    dynamicLightLocations.dynamicLightIntensities =
            GetShaderLocation(shader, "dynamicLightIntensities[0]");
    dynamicLightLocations.dynamicLightTypes =
            GetShaderLocation(shader, "dynamicLightTypes[0]");
    dynamicLightLocations.dynamicLightDirections =
            GetShaderLocation(shader, "dynamicLightDirections[0]");
    dynamicLightLocations.dynamicLightInnerConeCos =
            GetShaderLocation(shader, "dynamicLightInnerConeCos[0]");
    dynamicLightLocations.dynamicLightOuterConeCos =
            GetShaderLocation(shader, "dynamicLightOuterConeCos[0]");
    staticSpecularLocations = GetSectorStaticSpecularShaderLocations(shader);
    fogLocations = GetSectorFogShaderLocations(shader);

    material = LoadMaterialDefault();
    material.shader = shader;
    materialLoaded = true;
    cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    meshLoaded = cube.vertexCount > 0;
    return meshLoaded;
}

void SectorWindowRenderer::Shutdown()
{
    drawItems.clear();
    if (meshLoaded) UnloadMesh(cube);
    cube = {};
    meshLoaded = false;
    if (materialLoaded) {
        material.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
        UnloadMaterial(material);
    } else if (shader.id != 0) {
        UnloadShader(shader);
    }
    material = {};
    shader = {};
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

bool SectorWindowRenderer::HasVisibleWindows(
        engine::World& world,
        const RuntimePortalVisibilityResult* visibility) const
{
    bool found = false;
    world.ForEach<SectorObject, SectorWindow>(
            [&](engine::Entity, SectorObject& object, SectorWindow& window) {
                if (object.visible && window.visible
                        && WindowVisible(window, visibility)) found = true;
            });
    return found;
}

void SectorWindowRenderer::Draw(const SectorWindowDrawContext& context)
{
    consideredCount = 0;
    drawnCount = 0;
    localEnvironmentCount = 0;
    globalEnvironmentCount = 0;
    missingEnvironmentCount = 0;
    drawItems.clear();
    if (!materialLoaded || !meshLoaded || shader.id == 0
            || context.assets == nullptr || context.world == nullptr) return;

    context.world->ForEach<SectorObjectTransform, SectorObject,
            SectorObjectLighting, SectorWindow>(
            [&](engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorObjectLighting&,
                    SectorWindow& window) {
                ++consideredCount;
                if (!object.visible || !window.visible
                        || !WindowVisible(window, context.visibility)) return;
                const Vector3 delta = Vector3Subtract(
                        transform.position, context.camera.position);
                drawItems.push_back(DrawItem{
                        entity, window.placedObjectId,
                        Vector3LengthSqr(delta)});
            });
    std::sort(drawItems.begin(), drawItems.end(), [](const DrawItem& a,
            const DrawItem& b) {
        if (a.distanceSquared != b.distanceSquared) {
            return a.distanceSquared > b.distanceSquared;
        }
        return a.placedObjectId < b.placedObjectId;
    });

    if (cameraPositionLoc >= 0) SetShaderValue(
            shader, cameraPositionLoc, &context.camera.position,
            SHADER_UNIFORM_VEC3);
    const int advancedTransmission = context.advancedTransmission
                    && context.sceneColor != nullptr
                    && context.sceneDepth != nullptr
            ? 1 : 0;
    const Texture2D originalDiffuse =
            material.maps[MATERIAL_MAP_DIFFUSE].texture;
    const Texture2D originalSpecular =
            material.maps[MATERIAL_MAP_SPECULAR].texture;
    if (advancedTransmission != 0) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = *context.sceneColor;
        material.maps[MATERIAL_MAP_SPECULAR].texture = *context.sceneDepth;
    }
    if (advancedTransmissionLoc >= 0) SetShaderValue(
            shader, advancedTransmissionLoc, &advancedTransmission,
            SHADER_UNIFORM_INT);
    if (viewportSizeLoc >= 0) SetShaderValue(
            shader, viewportSizeLoc, &context.viewportSize,
            SHADER_UNIFORM_VEC2);
    const Matrix viewMatrix = GetCameraMatrix(context.camera);
    const Matrix projectionMatrix = rlGetMatrixProjection();
    if (viewMatrixLoc >= 0) SetShaderValueMatrix(
            shader, viewMatrixLoc, viewMatrix);
    if (projectionMatrixLoc >= 0) SetShaderValueMatrix(
            shader, projectionMatrixLoc, projectionMatrix);
    const SectorPbrContributionSettings pbr =
            NormalizeSectorPbrContributionSettings(context.pbr);
    if (environmentSpecularScaleLoc >= 0) SetShaderValue(
            shader, environmentSpecularScaleLoc,
            &pbr.worldEnvironmentSpecularScale, SHADER_UNIFORM_FLOAT);
    UploadSectorRendererDynamicPointLights(
            shader, dynamicLightLocations, context.dynamicLights);
    UploadSectorFogShaderValues(shader, fogLocations, context.fog);

    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY);
    if (advancedTransmission != 0) rlDisableDepthTest();
    else rlEnableDepthTest();
    rlDisableDepthMask();
    rlEnableBackfaceCulling();

    for (const DrawItem& item : drawItems) {
        if (!context.world->IsAlive(item.entity)
                || !context.world->Has<SectorObjectTransform>(item.entity)
                || !context.world->Has<SectorObject>(item.entity)
                || !context.world->Has<SectorObjectLighting>(item.entity)
                || !context.world->Has<SectorWindow>(item.entity)) continue;
        const SectorObjectTransform& transform =
                context.world->Get<SectorObjectTransform>(item.entity);
        const SectorObject& object = context.world->Get<SectorObject>(item.entity);
        const SectorObjectLighting& lighting =
                context.world->Get<SectorObjectLighting>(item.entity);
        const SectorWindow& window = context.world->Get<SectorWindow>(item.entity);

        const Vector3 tint = engine::SrgbColorBytesToLinearSceneRgb(window.tint);
        const Vector3 ambient = WindowAmbient(lighting);
        if (tintLoc >= 0) SetShaderValue(
                shader, tintLoc, &tint, SHADER_UNIFORM_VEC3);
        if (opacityLoc >= 0) SetShaderValue(
                shader, opacityLoc, &window.opacity, SHADER_UNIFORM_FLOAT);
        if (roughnessLoc >= 0) SetShaderValue(
                shader, roughnessLoc, &window.roughness, SHADER_UNIFORM_FLOAT);
        if (iorLoc >= 0) SetShaderValue(
                shader, iorLoc, &window.indexOfRefraction,
                SHADER_UNIFORM_FLOAT);
        if (thicknessLoc >= 0) SetShaderValue(
                shader, thicknessLoc, &window.thickness,
                SHADER_UNIFORM_FLOAT);
        if (ambientLoc >= 0) SetShaderValue(
                shader, ambientLoc, &ambient, SHADER_UNIFORM_VEC3);

        SectorPbrEnvironmentSelection selection;
        if (context.environment != nullptr) {
            selection = SelectWindowEnvironment(
                    *context.environment,
                    transform,
                    object,
                    window,
                    context.camera.position,
                    context.localReflectionProbesCurrent);
        }
        const TextureCubemap* cubemap = context.assets->GetCubemap(
                selection.cubemap);
        const int hasEnvironment = cubemap != nullptr && cubemap->id != 0
                        && pbr.worldEnvironmentSpecularScale > 0.0f
                ? 1 : 0;
        material.maps[MATERIAL_MAP_CUBEMAP].texture = hasEnvironment != 0
                ? *cubemap : Texture2D{};
        if (hasEnvironmentLoc >= 0) SetShaderValue(
                shader, hasEnvironmentLoc, &hasEnvironment,
                SHADER_UNIFORM_INT);
        const int boxProjection = selection.boxProjection ? 1 : 0;
        if (environmentBoxProjectionLoc >= 0) SetShaderValue(
                shader, environmentBoxProjectionLoc, &boxProjection,
                SHADER_UNIFORM_INT);
        if (environmentCapturePositionLoc >= 0) SetShaderValue(
                shader, environmentCapturePositionLoc,
                &selection.capturePosition, SHADER_UNIFORM_VEC3);
        if (environmentInfluenceCenterLoc >= 0) SetShaderValue(
                shader, environmentInfluenceCenterLoc,
                &selection.influenceCenter, SHADER_UNIFORM_VEC3);
        if (environmentHalfExtentsLoc >= 0) SetShaderValue(
                shader, environmentHalfExtentsLoc,
                &selection.halfExtents, SHADER_UNIFORM_VEC3);
        if (environmentYawLoc >= 0) SetShaderValue(
                shader, environmentYawLoc, &selection.yawRadians,
                SHADER_UNIFORM_FLOAT);
        if (environmentMaxLodLoc >= 0) SetShaderValue(
                shader, environmentMaxLodLoc, &selection.maxLod,
                SHADER_UNIFORM_FLOAT);
        const float environmentIntensity = selection.localProbe
                ? selection.intensity : 0.15f;
        if (environmentIntensityLoc >= 0) SetShaderValue(
                shader, environmentIntensityLoc, &environmentIntensity,
                SHADER_UNIFORM_FLOAT);
        if (hasEnvironment == 0) ++missingEnvironmentCount;
        else if (selection.localProbe) ++localEnvironmentCount;
        else ++globalEnvironmentCount;

        SectorStaticSpecularLightContext staticLights;
        if (context.staticSpecularLights != nullptr) {
            const Vector3 half{
                    window.width * 0.5f,
                    window.height * 0.5f,
                    window.thickness * 0.5f};
            const float horizontalRadius = std::sqrt(
                    half.x * half.x + half.z * half.z);
            const SectorReceiverBounds bounds{
                    object.currentSectorId,
                    Vector3{transform.position.x - horizontalRadius,
                            transform.position.y - half.y,
                            transform.position.z - horizontalRadius},
                    Vector3{transform.position.x + horizontalRadius,
                            transform.position.y + half.y,
                            transform.position.z + horizontalRadius}};
            const RuntimePortalVisibilityResult emptyVisibility;
            staticLights = SelectSectorStaticSpecularLights(
                    *context.staticSpecularLights,
                    bounds,
                    object.currentSectorId,
                    context.visibility != nullptr
                            ? *context.visibility : emptyVisibility,
                    context.staticSpecularEligible);
        }
        UploadSectorStaticSpecularLights(
                shader, staticSpecularLocations, staticLights);
        DrawMesh(cube, material,
                BuildSectorWindowModelMatrix(transform, window));
        ++drawnCount;
    }

    rlDrawRenderBatchActive();
    material.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
    material.maps[MATERIAL_MAP_DIFFUSE].texture = originalDiffuse;
    material.maps[MATERIAL_MAP_SPECULAR].texture = originalSpecular;
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    if (context.renderDebugText != nullptr) {
        *context.renderDebugText += " | windows: "
                + std::to_string(drawnCount) + " drawn / "
                + std::to_string(consideredCount) + " considered; env local/global/none "
                + std::to_string(localEnvironmentCount) + "/"
                + std::to_string(globalEnvironmentCount) + "/"
                + std::to_string(missingEnvironmentCount)
                + (advancedTransmission != 0 ? "; advanced" : "; simple");
    }
}

} // namespace game
