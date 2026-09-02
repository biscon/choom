#include "sector_demo/renderer/SectorLiquidRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

const char* LiquidVs = R"glsl(
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

const char* LiquidFs = R"glsl(
#version 330
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

uniform vec3 cameraPosition;
uniform float runtimeSeconds;
uniform vec3 shallowColor;
uniform vec3 deepColor;
uniform vec3 foamColor;
// visibility depth, roughness, refraction strength, foam amount
uniform vec4 liquidParams0;
// ripple scale, ripple strength, ripple speed, unused
uniform vec4 liquidParams1;
// direction radians, flow speed
uniform vec2 flowParams;
uniform int advancedTransmission;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform mat4 matInverseView;
uniform mat4 matInverseProjection;
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

vec3 ReconstructWorldPosition(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = matInverseProjection * clip;
    view /= max(abs(view.w), 0.000001);
    vec4 world = matInverseView * vec4(view.xyz, 1.0);
    return world.xyz;
}

vec2 WaveGradient(vec2 position, vec2 direction, float frequency,
        float phase, float amplitude)
{
    float angle = dot(position, direction) * frequency + phase;
    return direction * cos(angle) * frequency * amplitude;
}

vec3 ProceduralNormal()
{
    float scale = max(liquidParams1.x, 0.05);
    float strength = max(liquidParams1.y, 0.0);
    float phase = runtimeSeconds * liquidParams1.z;
    vec2 flowDirection = vec2(cos(flowParams.x), sin(flowParams.x));
    vec2 position = fragWorldPosition.xz
            - flowDirection * runtimeSeconds * flowParams.y;
    vec2 gradient = vec2(0.0);
    gradient += WaveGradient(position, normalize(vec2(0.93, 0.37)), 6.28318 / scale, phase * 1.17, 0.018);
    gradient += WaveGradient(position, normalize(vec2(-0.31, 0.95)), 6.28318 / (scale * 0.57), phase * 1.73 + 1.8, 0.008);
    gradient += WaveGradient(position, normalize(vec2(0.66, -0.75)), 6.28318 / (scale * 0.31), phase * 2.41 + 3.2, 0.003);
    gradient += WaveGradient(position, normalize(vec2(-0.82, -0.57)), 6.28318 / (scale * 1.8), phase * 0.63 + 4.7, 0.025);
    gradient *= strength;
    return SafeNormalize(vec3(-gradient.x, 1.0, -gradient.y), vec3(0.0, 1.0, 0.0));
}

vec3 RotateEnvironment(vec3 direction, float radians)
{
    float c = cos(radians);
    float s = sin(radians);
    return vec3(c * direction.x - s * direction.z,
            direction.y,
            s * direction.x + c * direction.z);
}

vec3 EnvironmentDirection(vec3 direction)
{
    if (environmentBoxProjection == 0) {
        return RotateEnvironment(direction, -environmentYaw);
    }
    float c = cos(-environmentYaw);
    float s = sin(-environmentYaw);
    vec3 origin = fragWorldPosition - environmentInfluenceCenter;
    vec3 localOrigin = vec3(origin.x*c-origin.z*s, origin.y, origin.x*s+origin.z*c);
    vec3 localDirection = vec3(direction.x*c-direction.z*s, direction.y, direction.x*s+direction.z*c);
    vec3 safeDirection = mix(vec3(-1.0), vec3(1.0), step(vec3(0.0), localDirection))
            * max(abs(localDirection), vec3(0.00001));
    vec3 exitPlane = mix(-environmentHalfExtents, environmentHalfExtents,
            step(vec3(0.0), localDirection));
    vec3 exitDistance = (exitPlane - localOrigin) / safeDirection;
    float distanceToBox = min(exitDistance.x, min(exitDistance.y, exitDistance.z));
    vec3 localHit = localOrigin + localDirection * max(distanceToBox, 0.0);
    vec3 captureOffset = environmentCapturePosition - environmentInfluenceCenter;
    vec3 localCapture = vec3(captureOffset.x*c-captureOffset.z*s,
            captureOffset.y, captureOffset.x*s+captureOffset.z*c);
    vec3 localLookup = localHit - localCapture;
    c = cos(environmentYaw);
    s = sin(environmentYaw);
    return SafeNormalize(vec3(localLookup.x*c-localLookup.z*s,
            localLookup.y, localLookup.x*s+localLookup.z*c), direction);
}

float FogAmount()
{
    if (fogEnabled == 0) return 0.0;
    float distanceFromCamera = length(fragWorldPosition - fogCameraPosition);
    float distancePastStart = max(distanceFromCamera - fogStartDistanceWorld, 0.0);
    float distanceFog = 1.0 - exp(-fogDensity * distancePastStart);
    float heightAttenuation = exp(-max(fragWorldPosition.y - fogReferenceHeightWorld, 0.0)
            * fogHeightFalloff);
    return clamp(distanceFog * heightAttenuation, 0.0, fogMaxOpacity);
}

vec3 DirectionalHighlight(vec3 normal, vec3 viewDirection, float roughness)
{
    if (directionalLightEnabled == 0 || directionalLightIntensity <= 0.0) return vec3(0.0);
    vec3 lightDirection = SafeNormalize(directionalLightDirection, vec3(0.0, 1.0, 0.0));
    float ndotl = max(dot(normal, lightDirection), 0.0);
    if (ndotl <= 0.0) return vec3(0.0);
    vec3 halfway = SafeNormalize(viewDirection + lightDirection, normal);
    float exponent = mix(256.0, 8.0, clamp(roughness, 0.0, 1.0));
    float highlight = pow(max(dot(normal, halfway), 0.0), exponent)
            * mix(1.0, 0.18, roughness);
    return directionalLightColor * directionalLightIntensity * highlight * ndotl;
}

void main()
{
    vec2 baseUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    if (advancedTransmission != 0) {
        float opaqueDepth = texture(sceneDepth, baseUv).r;
        if (opaqueDepth + 0.0005 < gl_FragCoord.z) discard;
    }

    vec3 normal = ProceduralNormal();
    vec3 viewDirection = SafeNormalize(cameraPosition - fragWorldPosition,
            vec3(0.0, 1.0, 0.0));
    float ndotv = clamp(dot(normal, viewDirection), 0.0, 1.0);
    float fresnel = 0.02 + 0.98 * pow(1.0 - ndotv, 5.0);
    float roughness = clamp(liquidParams0.y, 0.02, 1.0);

    vec3 reflection = mix(deepColor, shallowColor, 0.35);
    if (hasEnvironment != 0) {
        vec3 reflectedDirection = reflect(-viewDirection, normal);
        reflection = textureLod(environmentTexture,
                EnvironmentDirection(reflectedDirection),
                roughness * environmentMaxLod).rgb
                * environmentIntensity * environmentSpecularScale;
    }
    reflection += DirectionalHighlight(normal, viewDirection, roughness);

    float opticalDepth = liquidParams0.x;
    vec3 transmitted = shallowColor;
    float foam = 0.0;
    if (advancedTransmission != 0) {
        vec2 distortion = normal.xz * liquidParams0.z
                * mix(0.25, 1.0, 1.0 - ndotv);
        vec2 refractedUv = clamp(baseUv + distortion,
                vec2(0.001), vec2(0.999));
        float refractedDepth = texture(sceneDepth, refractedUv).r;
        if (refractedDepth + 0.0005 < gl_FragCoord.z) {
            refractedUv = baseUv;
            refractedDepth = texture(sceneDepth, baseUv).r;
        }
        vec3 opaquePosition = ReconstructWorldPosition(refractedUv, refractedDepth);
        opticalDepth = refractedDepth >= 0.99999
                ? liquidParams0.x
                : max(length(opaquePosition - fragWorldPosition), 0.0);
        float absorption = 1.0 - exp(-opticalDepth / max(liquidParams0.x, 0.05));
        vec3 sceneTransmission = texture(sceneColor, refractedUv).rgb;
        vec3 absorptionTint = mix(vec3(1.0), deepColor, absorption * 0.88);
        transmitted = mix(sceneTransmission * absorptionTint,
                deepColor, absorption * absorption * 0.72);
        float foamWidth = mix(0.02, 0.65, liquidParams0.w);
        foam = (1.0 - smoothstep(0.0, foamWidth, opticalDepth)) * liquidParams0.w;
        float crest = 0.5 + 0.5 * sin((fragWorldPosition.x + fragWorldPosition.z) * 13.0
                + runtimeSeconds * 1.9);
        foam *= mix(0.72, 1.0, crest);
    }

    vec3 rgb = mix(transmitted, reflection, fresnel);
    rgb = mix(rgb, foamColor, clamp(foam, 0.0, 1.0));
    rgb = mix(rgb, fogColor, FogAmount());
    rgb = clamp(rgb, vec3(0.0), vec3(65504.0));
    if (advancedTransmission != 0) {
        finalColor = vec4(rgb, 1.0);
    } else {
        float alpha = clamp(0.42 + fresnel * 0.45
                + (1.0 / max(liquidParams0.x, 0.05)) * 0.08, 0.35, 0.92);
        finalColor = vec4(rgb * alpha, alpha);
    }
}
)glsl";

bool VisibleSector(int sectorId, const RuntimePortalVisibilityResult* visibility)
{
    return visibility == nullptr
            || !visibility->validStartSector
            || visibility->fallbackDrawAll
            || ShouldDrawRuntimeSectorForVisibility(sectorId, *visibility);
}

template<typename SurfaceT>
void UnloadSurfaces(std::vector<SurfaceT>& surfaces)
{
    for (SurfaceT& surface : surfaces) {
        if (surface.mesh.vertexCount > 0) UnloadMesh(surface.mesh);
        surface.mesh = {};
    }
    surfaces.clear();
}

} // namespace

bool SectorLiquidRenderer::Initialize(std::size_t capacity)
{
    Shutdown();
    Reserve(capacity);
    shader = LoadShaderFromMemory(LiquidVs, LiquidFs);
    if (shader.id == 0) return false;
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocationAttrib(shader, "vertexNormal");
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "sceneColor");
    shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(shader, "sceneDepth");
    shader.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(shader, "environmentTexture");
    cameraPositionLoc = GetShaderLocation(shader, "cameraPosition");
    runtimeSecondsLoc = GetShaderLocation(shader, "runtimeSeconds");
    shallowColorLoc = GetShaderLocation(shader, "shallowColor");
    deepColorLoc = GetShaderLocation(shader, "deepColor");
    foamColorLoc = GetShaderLocation(shader, "foamColor");
    liquidParams0Loc = GetShaderLocation(shader, "liquidParams0");
    liquidParams1Loc = GetShaderLocation(shader, "liquidParams1");
    flowParamsLoc = GetShaderLocation(shader, "flowParams");
    advancedTransmissionLoc = GetShaderLocation(shader, "advancedTransmission");
    viewportSizeLoc = GetShaderLocation(shader, "viewportSize");
    inverseViewMatrixLoc = GetShaderLocation(shader, "matInverseView");
    inverseProjectionMatrixLoc = GetShaderLocation(shader, "matInverseProjection");
    hasEnvironmentLoc = GetShaderLocation(shader, "hasEnvironment");
    environmentBoxProjectionLoc = GetShaderLocation(shader, "environmentBoxProjection");
    environmentCapturePositionLoc = GetShaderLocation(shader, "environmentCapturePosition");
    environmentInfluenceCenterLoc = GetShaderLocation(shader, "environmentInfluenceCenter");
    environmentHalfExtentsLoc = GetShaderLocation(shader, "environmentHalfExtents");
    environmentYawLoc = GetShaderLocation(shader, "environmentYaw");
    environmentMaxLodLoc = GetShaderLocation(shader, "environmentMaxLod");
    environmentIntensityLoc = GetShaderLocation(shader, "environmentIntensity");
    environmentSpecularScaleLoc = GetShaderLocation(shader, "environmentSpecularScale");
    directionalLightEnabledLoc = GetShaderLocation(shader, "directionalLightEnabled");
    directionalLightDirectionLoc = GetShaderLocation(shader, "directionalLightDirection");
    directionalLightColorLoc = GetShaderLocation(shader, "directionalLightColor");
    directionalLightIntensityLoc = GetShaderLocation(shader, "directionalLightIntensity");
    fogLocations = GetSectorFogShaderLocations(shader);
    material = LoadMaterialDefault();
    material.shader = shader;
    materialLoaded = true;
    return true;
}

void SectorLiquidRenderer::Reserve(std::size_t capacity)
{
    surfaces.reserve(capacity);
    drawItems.reserve(capacity);
}

bool SectorLiquidRenderer::Rebuild(
        const SectorTopologyMap& map,
        const SectorGeneratedGeometry& geometry,
        std::string& error)
{
    error.clear();
    if (!materialLoaded) {
        error = "Liquid renderer is not initialized";
        return false;
    }
    std::vector<Surface> candidates;
    candidates.reserve(map.sectors.size());
    for (const SectorTopologySector& sector : map.sectors) {
        const SectorLiquidSettings settings = NormalizeSectorLiquidSettingsForSpan(
                sector.liquid, sector.floorZ, sector.ceilingZ);
        const float surfaceHeight = ResolveSectorLiquidSurfaceHeight(
                settings, sector.floorZ, sector.ceilingZ);
        if (!settings.enabled || surfaceHeight - sector.floorZ <= 0.0001f) continue;
        const float surfaceY = SectorAuthoringToWorldDistance(surfaceHeight);
        for (const SectorGeneratedSurface& generated : geometry.surfaces) {
            if (generated.ref.sourceKind != SectorGeneratedSurfaceSourceKind::Topology
                    || generated.ref.kind != SectorGeneratedSurfaceKind::Floor
                    || generated.ref.topologySectorId != sector.id
                    || generated.vertices.size() < 3) continue;
            Surface candidate;
            candidate.sectorId = sector.id;
            candidate.settings = settings;
            candidate.surfaceY = surfaceY;
            candidate.mesh.vertexCount = static_cast<int>(generated.vertices.size());
            candidate.mesh.triangleCount = candidate.mesh.vertexCount / 3;
            candidate.mesh.vertices = static_cast<float*>(MemAlloc(
                    generated.vertices.size() * 3 * sizeof(float)));
            candidate.mesh.normals = static_cast<float*>(MemAlloc(
                    generated.vertices.size() * 3 * sizeof(float)));
            if (candidate.mesh.vertices == nullptr || candidate.mesh.normals == nullptr) {
                if (candidate.mesh.vertices != nullptr) MemFree(candidate.mesh.vertices);
                if (candidate.mesh.normals != nullptr) MemFree(candidate.mesh.normals);
                UnloadSurfaces(candidates);
                error = "Could not allocate liquid surface mesh";
                return false;
            }
            Vector3 center{};
            for (std::size_t i = 0; i < generated.vertices.size(); ++i) {
                const Vector3 position{
                        generated.vertices[i].position.x,
                        surfaceY,
                        generated.vertices[i].position.z};
                candidate.mesh.vertices[i * 3 + 0] = position.x;
                candidate.mesh.vertices[i * 3 + 1] = position.y;
                candidate.mesh.vertices[i * 3 + 2] = position.z;
                candidate.mesh.normals[i * 3 + 0] = 0.0f;
                candidate.mesh.normals[i * 3 + 1] = 1.0f;
                candidate.mesh.normals[i * 3 + 2] = 0.0f;
                center = Vector3Add(center, position);
            }
            candidate.center = Vector3Scale(
                    center, 1.0f / static_cast<float>(generated.vertices.size()));
            UploadMesh(&candidate.mesh, false);
            candidates.push_back(std::move(candidate));
        }
    }
    UnloadSurfaces(surfaces);
    surfaces = std::move(candidates);
    drawItems.reserve(std::max(drawItems.capacity(), surfaces.size()));
    return true;
}

void SectorLiquidRenderer::Shutdown()
{
    UnloadSurfaces(surfaces);
    drawItems.clear();
    if (materialLoaded) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
        material.maps[MATERIAL_MAP_SPECULAR].texture = {};
        material.maps[MATERIAL_MAP_CUBEMAP].texture = {};
        UnloadMaterial(material);
    } else if (shader.id != 0) {
        UnloadShader(shader);
    }
    material = {};
    shader = {};
    materialLoaded = false;
    drawnCount = 0;
}

bool SectorLiquidRenderer::HasVisibleLiquids(
        const RuntimePortalVisibilityResult* visibility,
        Vector3 cameraPosition) const
{
    for (const Surface& surface : surfaces) {
        if (cameraPosition.y > surface.surfaceY + 0.001f
                && VisibleSector(surface.sectorId, visibility)) return true;
    }
    return false;
}

void SectorLiquidRenderer::Draw(const SectorLiquidDrawContext& context)
{
    drawnCount = 0;
    drawItems.clear();
    if (!materialLoaded || shader.id == 0 || context.assets == nullptr) return;
    for (std::size_t i = 0; i < surfaces.size(); ++i) {
        const Surface& surface = surfaces[i];
        if (context.camera.position.y <= surface.surfaceY + 0.001f
                || !VisibleSector(surface.sectorId, context.visibility)) continue;
        drawItems.push_back(DrawItem{i, Vector3DistanceSqr(
                context.camera.position, surface.center)});
    }
    std::sort(drawItems.begin(), drawItems.end(), [](const DrawItem& a, const DrawItem& b) {
        if (a.distanceSquared != b.distanceSquared) return a.distanceSquared > b.distanceSquared;
        return a.surfaceIndex < b.surfaceIndex;
    });
    if (drawItems.empty()) return;

    if (cameraPositionLoc >= 0) SetShaderValue(shader, cameraPositionLoc,
            &context.camera.position, SHADER_UNIFORM_VEC3);
    if (runtimeSecondsLoc >= 0) SetShaderValue(shader, runtimeSecondsLoc,
            &context.runtimeSeconds, SHADER_UNIFORM_FLOAT);
    const int advanced = context.advancedTransmission
                    && context.sceneColor != nullptr && context.sceneDepth != nullptr
            ? 1 : 0;
    if (advancedTransmissionLoc >= 0) SetShaderValue(shader, advancedTransmissionLoc,
            &advanced, SHADER_UNIFORM_INT);
    if (viewportSizeLoc >= 0) SetShaderValue(shader, viewportSizeLoc,
            &context.viewportSize, SHADER_UNIFORM_VEC2);
    const Matrix inverseView = MatrixInvert(GetCameraMatrix(context.camera));
    const Matrix inverseProjection = MatrixInvert(rlGetMatrixProjection());
    if (inverseViewMatrixLoc >= 0) SetShaderValueMatrix(shader, inverseViewMatrixLoc, inverseView);
    if (inverseProjectionMatrixLoc >= 0) SetShaderValueMatrix(shader, inverseProjectionMatrixLoc, inverseProjection);
    const SectorPbrContributionSettings pbr = NormalizeSectorPbrContributionSettings(context.pbr);
    if (environmentSpecularScaleLoc >= 0) SetShaderValue(shader,
            environmentSpecularScaleLoc, &pbr.worldEnvironmentSpecularScale,
            SHADER_UNIFORM_FLOAT);
    const SectorTopologyDirectionalLightSettings directional =
            NormalizeSectorTopologyDirectionalLightSettings(context.directionalLight);
    const int directionalEnabled = directional.enabled ? 1 : 0;
    const Vector3 directionalColor = engine::SrgbColorBytesToLinearSceneRgb(directional.color);
    if (directionalLightEnabledLoc >= 0) SetShaderValue(shader, directionalLightEnabledLoc,
            &directionalEnabled, SHADER_UNIFORM_INT);
    if (directionalLightDirectionLoc >= 0) SetShaderValue(shader, directionalLightDirectionLoc,
            &directional.directionToLight, SHADER_UNIFORM_VEC3);
    if (directionalLightColorLoc >= 0) SetShaderValue(shader, directionalLightColorLoc,
            &directionalColor, SHADER_UNIFORM_VEC3);
    if (directionalLightIntensityLoc >= 0) SetShaderValue(shader, directionalLightIntensityLoc,
            &directional.intensity, SHADER_UNIFORM_FLOAT);
    UploadSectorFogShaderValues(shader, fogLocations, context.fog);

    const Texture2D originalDiffuse = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    const Texture2D originalSpecular = material.maps[MATERIAL_MAP_SPECULAR].texture;
    if (advanced != 0) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = *context.sceneColor;
        material.maps[MATERIAL_MAP_SPECULAR].texture = *context.sceneDepth;
    }

    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY);
    if (advanced != 0) rlDisableDepthTest();
    else rlEnableDepthTest();
    rlDisableDepthMask();
    rlEnableBackfaceCulling();

    for (const DrawItem& item : drawItems) {
        Surface& surface = surfaces[item.surfaceIndex];
        const SectorLiquidSettings& liquid = surface.settings;
        const Vector3 shallow = engine::SrgbColorBytesToLinearSceneRgb(liquid.shallowColor);
        const Vector3 deep = engine::SrgbColorBytesToLinearSceneRgb(liquid.deepColor);
        const Vector3 foam = engine::SrgbColorBytesToLinearSceneRgb(liquid.foamColor);
        const Vector4 params0{liquid.visibilityDepthWorld, liquid.roughness,
                liquid.refractionStrength, liquid.foamAmount};
        const Vector4 params1{liquid.rippleScaleWorld, liquid.rippleStrength,
                liquid.rippleSpeed, 0.0f};
        const Vector2 flow{liquid.flowDirectionDegrees * DEG2RAD,
                liquid.flowSpeedWorld};
        if (shallowColorLoc >= 0) SetShaderValue(shader, shallowColorLoc, &shallow, SHADER_UNIFORM_VEC3);
        if (deepColorLoc >= 0) SetShaderValue(shader, deepColorLoc, &deep, SHADER_UNIFORM_VEC3);
        if (foamColorLoc >= 0) SetShaderValue(shader, foamColorLoc, &foam, SHADER_UNIFORM_VEC3);
        if (liquidParams0Loc >= 0) SetShaderValue(shader, liquidParams0Loc, &params0, SHADER_UNIFORM_VEC4);
        if (liquidParams1Loc >= 0) SetShaderValue(shader, liquidParams1Loc, &params1, SHADER_UNIFORM_VEC4);
        if (flowParamsLoc >= 0) SetShaderValue(shader, flowParamsLoc, &flow, SHADER_UNIFORM_VEC2);

        SectorPbrEnvironmentSelection selection;
        if (context.environment != nullptr) {
            selection = SelectSectorPbrEnvironment(*context.environment,
                    surface.center, surface.sectorId,
                    context.localReflectionProbesCurrent);
        }
        const TextureCubemap* cubemap = context.assets->GetCubemap(selection.cubemap);
        const int hasEnvironment = cubemap != nullptr && cubemap->id != 0
                        && pbr.worldEnvironmentSpecularScale > 0.0f
                ? 1 : 0;
        material.maps[MATERIAL_MAP_CUBEMAP].texture = hasEnvironment != 0
                ? *cubemap : Texture2D{};
        const int boxProjection = selection.boxProjection ? 1 : 0;
        const float intensity = selection.localProbe ? selection.intensity : 0.15f;
        if (hasEnvironmentLoc >= 0) SetShaderValue(shader, hasEnvironmentLoc, &hasEnvironment, SHADER_UNIFORM_INT);
        if (environmentBoxProjectionLoc >= 0) SetShaderValue(shader, environmentBoxProjectionLoc, &boxProjection, SHADER_UNIFORM_INT);
        if (environmentCapturePositionLoc >= 0) SetShaderValue(shader, environmentCapturePositionLoc, &selection.capturePosition, SHADER_UNIFORM_VEC3);
        if (environmentInfluenceCenterLoc >= 0) SetShaderValue(shader, environmentInfluenceCenterLoc, &selection.influenceCenter, SHADER_UNIFORM_VEC3);
        if (environmentHalfExtentsLoc >= 0) SetShaderValue(shader, environmentHalfExtentsLoc, &selection.halfExtents, SHADER_UNIFORM_VEC3);
        if (environmentYawLoc >= 0) SetShaderValue(shader, environmentYawLoc, &selection.yawRadians, SHADER_UNIFORM_FLOAT);
        if (environmentMaxLodLoc >= 0) SetShaderValue(shader, environmentMaxLodLoc, &selection.maxLod, SHADER_UNIFORM_FLOAT);
        if (environmentIntensityLoc >= 0) SetShaderValue(shader, environmentIntensityLoc, &intensity, SHADER_UNIFORM_FLOAT);
        DrawMesh(surface.mesh, material, MatrixIdentity());
        ++drawnCount;
    }

    rlDrawRenderBatchActive();
    material.maps[MATERIAL_MAP_CUBEMAP].texture = {};
    material.maps[MATERIAL_MAP_DIFFUSE].texture = originalDiffuse;
    material.maps[MATERIAL_MAP_SPECULAR].texture = originalSpecular;
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    if (context.renderDebugText != nullptr) {
        *context.renderDebugText += " | liquids: " + std::to_string(drawnCount)
                + " drawn / " + std::to_string(surfaces.size())
                + (advanced != 0 ? "; refractive" : "; fallback");
    }
}

} // namespace game
