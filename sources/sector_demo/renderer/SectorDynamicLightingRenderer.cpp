#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorStaticModelShadow.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <external/glad.h>
#include <raymath.h>
#include <rlgl.h>

namespace game {

namespace {

const SectorReceiverBounds* FindSectorReceiverBounds(
        const std::vector<SectorReceiverBounds>* bounds,
        int sectorId)
{
    if (bounds == nullptr || sectorId <= 0) return nullptr;
    for (const SectorReceiverBounds& candidate : *bounds) {
        if (candidate.sectorId == sectorId) return &candidate;
    }
    return nullptr;
}

bool SphereIntersectsAabb(
        Vector3 center,
        float radius,
        const SectorAabb3& bounds)
{
    if (!IsValidSectorAabb3(bounds)
            || !IsFiniteVector3(center)
            || !std::isfinite(radius)
            || radius <= 0.0f) {
        return true;
    }
    const Vector3 closest = ClosestPointOnSectorAabb3(bounds, center);
    return Vector3DistanceSqr(center, closest) <= radius * radius;
}

SectorAabb3 ToSectorAabb3(const SectorReceiverBounds& bounds)
{
    return SectorAabb3{bounds.min, bounds.max};
}

SectorAabb3 ToSectorAabb3(BoundingBox bounds)
{
    return SectorAabb3{bounds.min, bounds.max};
}

SectorAabb3 DoorCasterBounds(const SectorDoorShadowCaster& caster)
{
    const float radius = 0.5f * std::sqrt(
            caster.width * caster.width
            + caster.height * caster.height
            + caster.thickness * caster.thickness);
    return SectorAabb3{
            Vector3{
                    caster.position.x - radius,
                    caster.position.y - radius,
                    caster.position.z - radius},
            Vector3{
                    caster.position.x + radius,
                    caster.position.y + radius,
                    caster.position.z + radius}};
}

bool ShadowLightIntersectsBounds(
        const SectorPreviewDynamicPointLightUniform& light,
        const SectorPreviewDynamicSpotLightShadowMatrix& matrix,
        const SectorAabb3& bounds)
{
    if (!SphereIntersectsAabb(light.position, light.radius, bounds)) {
        return false;
    }
    if (light.kind == SectorPreviewDynamicLightKind::Point) {
        if (matrix.pointHemisphere > 0 && bounds.max.z < light.position.z) {
            return false;
        }
        if (matrix.pointHemisphere < 0 && bounds.min.z > light.position.z) {
            return false;
        }
        return true;
    }

    const Vector3 center = SectorAabb3Center(bounds);
    const Vector3 halfExtents = Vector3Scale(SectorAabb3Extents(bounds), 0.5f);
    const float boundsRadius = Vector3Length(halfExtents);
    const Vector3 fromLight = Vector3Subtract(center, light.position);
    const float distanceSquared = Vector3LengthSqr(fromLight);
    if (distanceSquared <= boundsRadius * boundsRadius) return true;
    const Vector3 direction = Vector3Normalize(light.direction);
    const float axialDistance = Vector3DotProduct(fromLight, direction);
    if (axialDistance < -boundsRadius
            || axialDistance > light.radius + boundsRadius) {
        return false;
    }
    const float radialDistance = std::sqrt(std::max(
            distanceSquared - axialDistance * axialDistance, 0.0f));
    const float halfAngle = std::acos(std::clamp(
            light.outerConeCos, -0.999f, 0.999f));
    const float clampedHalfAngle = std::min(halfAngle, 1.553343f);
    const float coneRadius = std::max(axialDistance, 0.0f)
            * std::tan(clampedHalfAngle);
    const float sphereAllowance = boundsRadius
            / std::max(std::cos(clampedHalfAngle), 0.017452f);
    return radialDistance <= coneRadius + sphereAllowance;
}

bool DynamicLightIntersectsBounds(
        const SectorPreviewDynamicPointLightUniform& light,
        const SectorAabb3& bounds)
{
    if (!SphereIntersectsAabb(light.position, light.radius, bounds)) {
        return false;
    }
    if (light.kind == SectorPreviewDynamicLightKind::Point) {
        return true;
    }

    const Vector3 center = SectorAabb3Center(bounds);
    const Vector3 halfExtents = Vector3Scale(SectorAabb3Extents(bounds), 0.5f);
    const float boundsRadius = Vector3Length(halfExtents);
    const Vector3 fromLight = Vector3Subtract(center, light.position);
    const float distanceSquared = Vector3LengthSqr(fromLight);
    if (distanceSquared <= boundsRadius * boundsRadius) return true;
    const Vector3 direction = Vector3Normalize(light.direction);
    const float axialDistance = Vector3DotProduct(fromLight, direction);
    if (axialDistance < -boundsRadius
            || axialDistance > light.radius + boundsRadius) {
        return false;
    }
    const float radialDistance = std::sqrt(std::max(
            distanceSquared - axialDistance * axialDistance, 0.0f));
    const float halfAngle = std::acos(std::clamp(
            light.outerConeCos, -0.999f, 0.999f));
    const float clampedHalfAngle = std::min(halfAngle, 1.553343f);
    const float coneRadius = std::max(axialDistance, 0.0f)
            * std::tan(clampedHalfAngle);
    const float sphereAllowance = boundsRadius
            / std::max(std::cos(clampedHalfAngle), 0.017452f);
    return radialDistance <= coneRadius + sphereAllowance;
}

const char* SectorSpotLightShadowVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;

uniform mat4 lightViewProjection;
uniform mat4 matModel;

out vec2 fragTexCoord;

void main()
{
    fragTexCoord = vertexTexCoord;
    gl_Position = lightViewProjection * matModel * vec4(vertexPosition, 1.0);
}
)";

const char* SectorSpotLightShadowFs = R"(
#version 330
in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform int alphaTest;
uniform float alphaCutoff;

void main()
{
    if (alphaTest != 0 && texture(texture0, fragTexCoord).a < alphaCutoff) {
        discard;
    }
}
)";

const char* SectorPointLightShadowVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
uniform mat4 matModel;
out vec3 worldPositionVs;
out vec2 texCoordVs;
void main()
{
    vec4 world = matModel * vec4(vertexPosition, 1.0);
    worldPositionVs = world.xyz;
    texCoordVs = vertexTexCoord;
    gl_Position = world;
}
)";

// Adaptive edge subdivision keeps the nonlinear paraboloid projection stable
// for large, low-poly sector triangles without changing source meshes. Each
// generated triangle is clipped to the active hemisphere before projection so
// triangles crossing the paraboloid seam cannot rasterize invalid footprints.
const char* SectorPointLightShadowGs = R"(
#version 330
layout(triangles) in;
layout(triangle_strip, max_vertices = 64) out;
in vec3 worldPositionVs[];
in vec2 texCoordVs[];
noperspective out vec2 fragParaboloidPosition;
noperspective out vec2 fragTexCoord;
flat out vec4 fragTrianglePlane;
uniform vec3 pointLightPosition;
uniform int pointHemisphere;

vec4 trianglePlane;

vec3 worldAt(vec3 bary)
{
    return worldPositionVs[0] * bary.x
            + worldPositionVs[1] * bary.y
            + worldPositionVs[2] * bary.z;
}
vec2 texCoordAt(vec3 bary)
{
    return texCoordVs[0] * bary.x
            + texCoordVs[1] * bary.y
            + texCoordVs[2] * bary.z;
}
void emitProjectedPoint(vec3 worldPosition, vec2 texCoord)
{
    fragTexCoord = texCoord;
    fragTrianglePlane = trianglePlane;
    vec3 local = worldPosition - pointLightPosition;
    float distanceToLight = max(length(local), 0.00001);
    float hemisphereZ = local.z * float(pointHemisphere);
    float denominator = distanceToLight + max(hemisphereZ, 0.0);
    vec2 projected = local.xy / max(denominator, 0.00001);
    fragParaboloidPosition = projected;
    gl_Position = vec4(projected, 0.0, 1.0);
    EmitVertex();
}

float hemisphereDistance(vec3 worldPosition)
{
    return (worldPosition.z - pointLightPosition.z) * float(pointHemisphere);
}

void clipAndEmitTriangle(
        vec3 world0, vec2 texCoord0,
        vec3 world1, vec2 texCoord1,
        vec3 world2, vec2 texCoord2)
{
    vec3 sourceWorld[3];
    vec2 sourceTexCoord[3];
    sourceWorld[0] = world0;
    sourceWorld[1] = world1;
    sourceWorld[2] = world2;
    sourceTexCoord[0] = texCoord0;
    sourceTexCoord[1] = texCoord1;
    sourceTexCoord[2] = texCoord2;

    vec3 clippedWorld[4];
    vec2 clippedTexCoord[4];
    int clippedCount = 0;
    for (int currentIndex = 0; currentIndex < 3; ++currentIndex) {
        int previousIndex = currentIndex == 0 ? 2 : currentIndex - 1;
        float previousDistance = hemisphereDistance(sourceWorld[previousIndex]);
        float currentDistance = hemisphereDistance(sourceWorld[currentIndex]);
        bool previousInside = previousDistance >= 0.0;
        bool currentInside = currentDistance >= 0.0;

        if (previousInside != currentInside) {
            float denominator = previousDistance - currentDistance;
            float amount = clamp(previousDistance / denominator, 0.0, 1.0);
            clippedWorld[clippedCount] = mix(
                    sourceWorld[previousIndex], sourceWorld[currentIndex], amount);
            clippedTexCoord[clippedCount] = mix(
                    sourceTexCoord[previousIndex], sourceTexCoord[currentIndex], amount);
            ++clippedCount;
        }
        if (currentInside) {
            clippedWorld[clippedCount] = sourceWorld[currentIndex];
            clippedTexCoord[clippedCount] = sourceTexCoord[currentIndex];
            ++clippedCount;
        }
    }

    if (clippedCount == 3) {
        emitProjectedPoint(clippedWorld[0], clippedTexCoord[0]);
        emitProjectedPoint(clippedWorld[1], clippedTexCoord[1]);
        emitProjectedPoint(clippedWorld[2], clippedTexCoord[2]);
        EndPrimitive();
    } else if (clippedCount == 4) {
        // A convex polygon ordered around its perimeter becomes a valid strip
        // when the final two vertices are swapped.
        emitProjectedPoint(clippedWorld[0], clippedTexCoord[0]);
        emitProjectedPoint(clippedWorld[1], clippedTexCoord[1]);
        emitProjectedPoint(clippedWorld[3], clippedTexCoord[3]);
        emitProjectedPoint(clippedWorld[2], clippedTexCoord[2]);
        EndPrimitive();
    }
}

void emitSubdividedTriangle(vec3 a, vec3 b, vec3 c)
{
    clipAndEmitTriangle(
            worldAt(a), texCoordAt(a),
            worldAt(b), texCoordAt(b),
            worldAt(c), texCoordAt(c));
}
void main()
{
    vec3 planeNormal = cross(
            worldPositionVs[1] - worldPositionVs[0],
            worldPositionVs[2] - worldPositionVs[0]);
    float planeNormalLengthSquared = dot(planeNormal, planeNormal);
    if (planeNormalLengthSquared <= 0.000000000001) return;
    planeNormal *= inversesqrt(planeNormalLengthSquared);
    trianglePlane = vec4(
            planeNormal,
            dot(planeNormal, worldPositionVs[0] - pointLightPosition));

    vec3 lightVector0 = worldPositionVs[0] - pointLightPosition;
    vec3 lightVector1 = worldPositionVs[1] - pointLightPosition;
    vec3 lightVector2 = worldPositionVs[2] - pointLightPosition;
    float lengthSquared0 = dot(lightVector0, lightVector0);
    float lengthSquared1 = dot(lightVector1, lightVector1);
    float lengthSquared2 = dot(lightVector2, lightVector2);
    float minimumDot = -1.0;
    if (lengthSquared0 > 0.00000001
            && lengthSquared1 > 0.00000001
            && lengthSquared2 > 0.00000001) {
        vec3 d0 = lightVector0 * inversesqrt(lengthSquared0);
        vec3 d1 = lightVector1 * inversesqrt(lengthSquared1);
        vec3 d2 = lightVector2 * inversesqrt(lengthSquared2);
        minimumDot = min(dot(d0, d1), min(dot(d1, d2), dot(d2, d0)));
    }
    int divisions = minimumDot > 0.995 ? 1 : (minimumDot > 0.96 ? 2 : 4);
    float inverseDivisions = 1.0 / float(divisions);
    for (int i = 0; i < divisions; ++i) {
        for (int j = 0; j < divisions - i; ++j) {
            vec3 a = vec3(float(divisions-i-j), float(i), float(j)) * inverseDivisions;
            vec3 b = vec3(float(divisions-i-j-1), float(i+1), float(j)) * inverseDivisions;
            vec3 c = vec3(float(divisions-i-j-1), float(i), float(j+1)) * inverseDivisions;
            emitSubdividedTriangle(a, b, c);
            if (j < divisions - i - 1) {
                vec3 d = vec3(float(divisions-i-j-2), float(i+1), float(j+1)) * inverseDivisions;
                emitSubdividedTriangle(b, d, c);
            }
        }
    }
}
)";

const char* SectorPointLightShadowFs = R"(
#version 330
noperspective in vec2 fragParaboloidPosition;
noperspective in vec2 fragTexCoord;
flat in vec4 fragTrianglePlane;
uniform sampler2D texture0;
uniform int alphaTest;
uniform float alphaCutoff;
uniform float pointLightRadius;
uniform int pointHemisphere;
void main()
{
    if (alphaTest != 0 && texture(texture0, fragTexCoord).a < alphaCutoff) discard;

    float projectedRadiusSquared = dot(fragParaboloidPosition, fragParaboloidPosition);
    if (projectedRadiusSquared > 1.00001) discard;
    projectedRadiusSquared = min(projectedRadiusSquared, 1.0);
    float inverseProjectionDenominator = 1.0 / (1.0 + projectedRadiusSquared);
    vec3 rayDirection = vec3(
            fragParaboloidPosition * (2.0 * inverseProjectionDenominator),
            (1.0 - projectedRadiusSquared) * inverseProjectionDenominator
                    * float(pointHemisphere));
    rayDirection = normalize(rayDirection);

    float planeDirection = dot(fragTrianglePlane.xyz, rayDirection);
    if (abs(planeDirection) <= 0.000001) discard;
    float distanceToLight = fragTrianglePlane.w / planeDirection;
    if (distanceToLight <= 0.00001 || distanceToLight > pointLightRadius) discard;
    gl_FragDepth = clamp(distanceToLight / pointLightRadius, 0.0, 1.0);
}
)";

unsigned int CompileShaderStage(unsigned int type, const char* source)
{
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;
    char log[2048]{};
    glGetShaderInfoLog(shader, static_cast<int>(sizeof(log)), nullptr, log);
    TraceLog(LOG_ERROR, "DPSM shader compile failed: %s", log);
    glDeleteShader(shader);
    return 0;
}

Shader LoadGeometryShader(const char* vertexSource, const char* geometrySource,
        const char* fragmentSource)
{
    const unsigned int vertex = CompileShaderStage(GL_VERTEX_SHADER, vertexSource);
    const unsigned int geometry = CompileShaderStage(GL_GEOMETRY_SHADER, geometrySource);
    const unsigned int fragment = CompileShaderStage(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertex == 0 || geometry == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (geometry != 0) glDeleteShader(geometry);
        if (fragment != 0) glDeleteShader(fragment);
        return Shader{};
    }
    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, geometry);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(geometry);
    glDeleteShader(fragment);
    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[2048]{};
        glGetProgramInfoLog(program, static_cast<int>(sizeof(log)), nullptr, log);
        TraceLog(LOG_ERROR, "DPSM shader link failed: %s", log);
        glDeleteProgram(program);
        return Shader{};
    }
    Shader result{};
    result.id = program;
    result.locs = static_cast<int*>(MemAlloc(RL_MAX_SHADER_LOCATIONS * sizeof(int)));
    if (result.locs == nullptr) {
        glDeleteProgram(program);
        return Shader{};
    }
    for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; ++i) result.locs[i] = -1;
    return result;
}

RenderTexture2D LoadDepthOnlyRenderTexture(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    target.texture.width = width;
    target.texture.height = height;

    if (target.id <= 0) {
        return target;
    }

    rlEnableFramebuffer(target.id);
    target.depth.id = rlLoadTextureDepth(width, height, false);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    if (!rlFramebufferComplete(target.id)) {
        rlDisableFramebuffer();
        rlUnloadFramebuffer(target.id);
        return RenderTexture2D{};
    }

    rlDisableFramebuffer();
    return target;
}

void UnloadDepthOnlyRenderTexture(RenderTexture2D& target)
{
    if (target.id > 0) {
        rlUnloadFramebuffer(target.id);
    }
    target = RenderTexture2D{};
}

} // namespace

void UploadSectorRendererDynamicPointLights(
        Shader shader,
        const SectorDynamicLightShaderLocations& locations,
        bool dynamicLightingEnabled,
        float runtimeSeconds,
        const std::vector<SectorPreviewDynamicPointLightUniform>& lights)
{
    const int lightCount = dynamicLightingEnabled
            ? static_cast<int>(std::min(lights.size(), static_cast<size_t>(MaxDynamicLights)))
            : 0;
    SectorBillboardDynamicLightContext context;
    context.dynamicLightCount = lightCount;
    if (lightCount <= 0) {
        UploadSectorRendererDynamicPointLights(shader, locations, context);
        return;
    }

    std::array<Vector3, MaxDynamicLights> positions{};
    std::array<Vector3, MaxDynamicLights> colors{};
    std::array<float, MaxDynamicLights> radii{};
    std::array<float, MaxDynamicLights> intensities{};
    std::array<int, MaxDynamicLights> types{};
    std::array<Vector3, MaxDynamicLights> directions{};
    std::array<float, MaxDynamicLights> innerConeCos{};
    std::array<float, MaxDynamicLights> outerConeCos{};
    for (int i = 0; i < lightCount; ++i) {
        positions[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].position;
        colors[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].color;
        radii[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].radius;
        intensities[static_cast<size_t>(i)] = DynamicLightEffectiveUploadIntensity(
                lights[static_cast<size_t>(i)],
                runtimeSeconds);
        types[static_cast<size_t>(i)] = static_cast<int>(lights[static_cast<size_t>(i)].kind);
        directions[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].direction;
        innerConeCos[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].innerConeCos;
        outerConeCos[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].outerConeCos;
    }

    context.dynamicLightPositions = positions;
    context.dynamicLightColors = colors;
    context.dynamicLightRadii = radii;
    context.dynamicLightIntensities = intensities;
    context.dynamicLightTypes = types;
    context.dynamicLightDirections = directions;
    context.dynamicLightInnerConeCos = innerConeCos;
    context.dynamicLightOuterConeCos = outerConeCos;
    UploadSectorRendererDynamicPointLights(shader, locations, context);
}

void UploadSectorRendererDynamicPointLights(
        Shader shader,
        const SectorDynamicLightShaderLocations& locations,
        const SectorBillboardDynamicLightContext& context)
{
    if (locations.dynamicLightCount >= 0) {
        SetShaderValue(shader, locations.dynamicLightCount, &context.dynamicLightCount, SHADER_UNIFORM_INT);
    }
    if (context.dynamicLightCount <= 0) {
        return;
    }

    if (locations.dynamicLightPositions >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightPositions,
                context.dynamicLightPositions.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightColors >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightColors,
                context.dynamicLightColors.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightRadii >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightRadii,
                context.dynamicLightRadii.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightIntensities >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightIntensities,
                context.dynamicLightIntensities.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightTypes >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightTypes,
                context.dynamicLightTypes.data(),
                SHADER_UNIFORM_INT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightDirections >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightDirections,
                context.dynamicLightDirections.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightInnerConeCos >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightInnerConeCos,
                context.dynamicLightInnerConeCos.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightOuterConeCos >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightOuterConeCos,
                context.dynamicLightOuterConeCos.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
}

void UploadSectorRendererDynamicSpotLightShadowUniforms(
        Shader shader,
        const SectorDynamicSpotLightShadowShaderLocations& locations,
        const SectorPreviewDynamicSpotLightShadowUniforms& uniforms)
{
    UploadSectorRendererDynamicShadowSlots(
            shader, locations.dynamicLightShadowSlots, uniforms);
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        if (locations.shadowLightMatrices[i] >= 0) {
            SetShaderValueMatrix(shader, locations.shadowLightMatrices[i], uniforms.shadowLightMatrices[i]);
        }
    }
    if (locations.shadowBias >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowBias,
                uniforms.shadowBias.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
    if (locations.shadowStrength >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowStrength,
                uniforms.shadowStrength.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
    if (locations.shadowSoftness >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowSoftness,
                uniforms.shadowSoftness.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
    if (locations.shadowAtlasTilesPerRow >= 0) {
        SetShaderValue(shader, locations.shadowAtlasTilesPerRow,
                &uniforms.shadowAtlasTilesPerRow, SHADER_UNIFORM_INT);
    }
}

void UploadSectorRendererDynamicShadowSlots(
        Shader shader,
        int dynamicLightShadowSlotsLocation,
        const SectorPreviewDynamicSpotLightShadowUniforms& uniforms)
{
    if (dynamicLightShadowSlotsLocation < 0) return;
    SetShaderValueV(
            shader,
            dynamicLightShadowSlotsLocation,
            uniforms.dynamicLightShadowSlots.data(),
            SHADER_UNIFORM_INT,
            static_cast<int>(MaxDynamicLights));
}

void SectorDynamicLightingRenderer::Reset()
{
    sources.clear();
    selectionSources.clear();
    runtimePointLight = {};
    runtimePointLightActive = false;
    candidates.clear();
    selectedLights.clear();
    selectedLightIds.clear();
    receiverBounds.clear();
    sectorLightContexts.clear();
    shadowCasters.clear();
    shadowMatrices.clear();
    cachedShadowMatrices.clear();
    shadowMapsCacheValid = false;
    cachedDoorShadowCasterRevision = 0;
    cachedStaticModelShadowCasterRevision = 0;
    shadowRenderStats = {};
}

void SectorDynamicLightingRenderer::BeginShadowFrame(bool enabled)
{
    shadowRenderStats = {};
    shadowRenderStats.enabled = enabled;
}

void SectorDynamicLightingRenderer::RebuildSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld)
{
    BuildSectorPreviewDynamicPointLightSources(map, sectorLookupWorld, sources);
    selectionSources.reserve(sources.size() + 1);
    ReserveSelectionBuffers();
}

void SectorDynamicLightingRenderer::ReserveReceiverBoundsCapacity(
        size_t sectorCapacity,
        size_t runtimeObjectCapacity)
{
    receiverBounds.clear();
    receiverBounds.reserve(sectorCapacity + runtimeObjectCapacity * 2);
    sectorLightContexts.clear();
    sectorLightContexts.reserve(sectorCapacity);
}

void SectorDynamicLightingRenderer::SetRuntimePointLight(
        const SectorPreviewDynamicPointLightSource* light)
{
    runtimePointLightActive = light != nullptr;
    runtimePointLight = light != nullptr
            ? *light
            : SectorPreviewDynamicPointLightSource{};
}

void SectorDynamicLightingRenderer::UpdateSelection(
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld,
        const RuntimeSectorVisibilityGraph* visibilityGraph,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers)
{
    BuildReceiverBounds(sectorReceiverBounds, runtimeObjectWorld);
    selectionSources.assign(sources.begin(), sources.end());
    if (runtimePointLightActive) selectionSources.push_back(runtimePointLight);
    CollectSectorPreviewDynamicPointLightCandidates(
            selectionSources,
            visibility,
            receiverBounds,
            candidates,
            visibilityGraph,
            dynamicPortalBlockers);
    SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visibility,
            receiverBounds,
            maxDynamicLights,
            selectedLights,
            &selectedLightIds,
            &selectedLightIds);
    SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selectedLights,
            visibility,
            receiverBounds,
            ShadowSlotBudget(),
            shadowCasters);
    BuildSectorPreviewDynamicSpotLightShadowMatrices(
            selectedLights,
            shadowCasters,
            shadowMatrices);
    bool matricesMatch = shadowMatrices.size() == cachedShadowMatrices.size();
    for (std::size_t i = 0; matricesMatch && i < shadowMatrices.size(); ++i) {
        const auto& current = shadowMatrices[i];
        const auto& cached = cachedShadowMatrices[i];
        matricesMatch = current.lightId == cached.lightId
                && current.shadowSlot == cached.shadowSlot
                && current.kind == cached.kind
                && current.pointHemisphere == cached.pointHemisphere
                && std::memcmp(&current.lightPosition,
                        &cached.lightPosition, sizeof(Vector3)) == 0
                && current.lightRadius == cached.lightRadius
                && std::memcmp(&current.lightViewProjection,
                        &cached.lightViewProjection, sizeof(Matrix)) == 0;
    }
    if (!matricesMatch) {
        shadowMapsCacheValid = false;
        cachedShadowMatrices.assign(shadowMatrices.begin(), shadowMatrices.end());
    }
}

SectorBillboardDynamicLightContext SectorDynamicLightingRenderer::BuildLightContext(
        const SectorReceiverBounds* bounds,
        bool dynamicLightingEnabled,
        bool shadowMapsEnabled,
        float runtimeSeconds) const
{
    SectorBillboardDynamicLightContext context;
    context.shadowUniforms = PackShadowUniforms(shadowMapsEnabled);
    const std::array<int, MaxDynamicLights> globalShadowSlots =
            context.shadowUniforms.dynamicLightShadowSlots;
    context.shadowUniforms.dynamicLightShadowSlots.fill(-1);
    context.shadowMaps = BuildShadowMapTextures(shadowMapsEnabled);
    if (!dynamicLightingEnabled) return context;

    const SectorAabb3 receiverAabb = bounds != nullptr
            ? ToSectorAabb3(*bounds)
            : SectorAabb3{};
    for (std::size_t selectedIndex = 0;
            selectedIndex < selectedLights.size()
                    && context.dynamicLightCount
                            < static_cast<int>(MaxDynamicLights);
            ++selectedIndex) {
        const SectorPreviewDynamicPointLightUniform& light =
                selectedLights[selectedIndex];
        if (bounds != nullptr
                && !DynamicLightIntersectsBounds(light, receiverAabb)) {
            continue;
        }

        const std::size_t localIndex =
                static_cast<std::size_t>(context.dynamicLightCount++);
        context.dynamicLightIds[localIndex] = light.lightId;
        context.dynamicLightPositions[localIndex] = light.position;
        context.dynamicLightColors[localIndex] = light.color;
        context.dynamicLightRadii[localIndex] = light.radius;
        context.dynamicLightIntensities[localIndex] =
                DynamicLightEffectiveUploadIntensity(light, runtimeSeconds);
        context.dynamicLightTypes[localIndex] = static_cast<int>(light.kind);
        context.dynamicLightDirections[localIndex] = light.direction;
        context.dynamicLightInnerConeCos[localIndex] = light.innerConeCos;
        context.dynamicLightOuterConeCos[localIndex] = light.outerConeCos;
        context.shadowUniforms.dynamicLightShadowSlots[localIndex] =
                globalShadowSlots[selectedIndex];
    }
    return context;
}

void SectorDynamicLightingRenderer::BuildSectorLightContexts(
        const std::vector<SectorReceiverBounds>& sectorBounds,
        bool dynamicLightingEnabled,
        bool shadowMapsEnabled,
        float runtimeSeconds)
{
    sectorLightContexts.clear();
    for (const SectorReceiverBounds& bounds : sectorBounds) {
        sectorLightContexts.push_back(SectorDynamicLightSectorContext{
                bounds.sectorId,
                BuildLightContext(
                        &bounds,
                        dynamicLightingEnabled,
                        shadowMapsEnabled,
                        runtimeSeconds)});
    }
}

const SectorBillboardDynamicLightContext*
SectorDynamicLightingRenderer::FindSectorLightContext(int sectorId) const
{
    for (const SectorDynamicLightSectorContext& context : sectorLightContexts) {
        if (context.sectorId == sectorId) return &context.lighting;
    }
    return nullptr;
}

SectorPreviewDynamicSpotLightShadowUniforms SectorDynamicLightingRenderer::PackShadowUniforms(
        bool enabled) const
{
    if (!enabled) {
        SectorPreviewDynamicSpotLightShadowUniforms result;
        result.dynamicLightShadowSlots.fill(-1);
        result.shadowAtlasTilesPerRow = DynamicShadowAtlasResolution / shadowMapResolution;
        return result;
    }
    SectorPreviewDynamicSpotLightShadowUniforms result =
            PackSectorPreviewDynamicSpotLightShadowUniforms(
                    selectedLights, shadowCasters, shadowMatrices);
    result.shadowAtlasTilesPerRow = DynamicShadowAtlasResolution / shadowMapResolution;
    return result;
}

bool SectorDynamicLightingRenderer::EnsureShadowMapResources()
{
    if (shadowAtlas.id != 0 && shadowAtlas.depth.id != 0) {
        return true;
    }
    shadowAtlas = LoadDepthOnlyRenderTexture(
            DynamicShadowAtlasResolution, DynamicShadowAtlasResolution);
    if (shadowAtlas.id == 0 || shadowAtlas.depth.id == 0) {
        UnloadShadowMapResources();
        return false;
    }
    SetTextureFilter(shadowAtlas.depth, TEXTURE_FILTER_POINT);
    SetTextureWrap(shadowAtlas.depth, TEXTURE_WRAP_CLAMP);
    return true;
}

void SectorDynamicLightingRenderer::SetShadowMapResolution(int resolution)
{
    resolution = std::clamp(resolution, 256, 2048);
    if (shadowMapResolution == resolution) {
        return;
    }
    shadowMapResolution = resolution;
    shadowMapsCacheValid = false;
    UnloadShadowMapResources();
    EnsureShadowMapResources();
}

void SectorDynamicLightingRenderer::UnloadShadowMapResources()
{
    UnloadDepthOnlyRenderTexture(shadowAtlas);
}

bool SectorDynamicLightingRenderer::HasShadowMapResources() const
{
    return shadowAtlas.id != 0 || shadowAtlas.depth.id != 0;
}

bool SectorDynamicLightingRenderer::LoadShadowMaterial()
{
    shadowMaterial = LoadMaterialDefault();
    Shader shader = LoadShaderFromMemory(SectorSpotLightShadowVs, SectorSpotLightShadowFs);
    if (shader.id == 0) {
        UnloadMaterial(shadowMaterial);
        shadowMaterial = Material{};
        return false;
    }
    pointShadowMaterial = LoadMaterialDefault();
    Shader pointShader = LoadGeometryShader(
            SectorPointLightShadowVs,
            SectorPointLightShadowGs,
            SectorPointLightShadowFs);
    if (pointShader.id == 0) {
        UnloadShader(shader);
        UnloadMaterial(shadowMaterial);
        UnloadMaterial(pointShadowMaterial);
        shadowMaterial = Material{};
        pointShadowMaterial = Material{};
        return false;
    }
    shadowMaterial.shader = shader;
    shadowMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(shadowMaterial.shader, "vertexPosition");
    shadowMaterial.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(shadowMaterial.shader, "vertexTexCoord");
    shadowMaterial.shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shadowMaterial.shader, "matModel");
    shadowMaterial.shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shadowMaterial.shader, "texture0");
    shadowLightViewProjectionLoc = GetShaderLocation(shadowMaterial.shader, "lightViewProjection");
    shadowAlphaTestLoc = GetShaderLocation(shadowMaterial.shader, "alphaTest");
    shadowAlphaCutoffLoc = GetShaderLocation(shadowMaterial.shader, "alphaCutoff");
    shadowDefaultTexture = shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    pointShadowMaterial.shader = pointShader;
    pointShadowMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(pointShader, "vertexPosition");
    pointShadowMaterial.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(pointShader, "vertexTexCoord");
    pointShadowMaterial.shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(pointShader, "matModel");
    pointShadowMaterial.shader.locs[SHADER_LOC_MAP_DIFFUSE] =
            GetShaderLocation(pointShader, "texture0");
    pointShadowLightPositionLoc = GetShaderLocation(pointShader, "pointLightPosition");
    pointShadowLightRadiusLoc = GetShaderLocation(pointShader, "pointLightRadius");
    pointShadowHemisphereLoc = GetShaderLocation(pointShader, "pointHemisphere");
    pointShadowAlphaTestLoc = GetShaderLocation(pointShader, "alphaTest");
    pointShadowAlphaCutoffLoc = GetShaderLocation(pointShader, "alphaCutoff");
    pointShadowDefaultTexture = pointShadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    shadowMaterialLoaded = true;
    return true;
}

void SectorDynamicLightingRenderer::UnloadShadowMaterial()
{
    if (!shadowMaterialLoaded) {
        return;
    }

    shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = shadowDefaultTexture;
    pointShadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = pointShadowDefaultTexture;
    UnloadMaterial(shadowMaterial);
    UnloadMaterial(pointShadowMaterial);
    shadowMaterial = Material{};
    pointShadowMaterial = Material{};
    shadowDefaultTexture = Texture2D{};
    pointShadowDefaultTexture = Texture2D{};
    shadowMaterialLoaded = false;
    shadowLightViewProjectionLoc = -1;
    shadowAlphaTestLoc = -1;
    shadowAlphaCutoffLoc = -1;
    pointShadowLightPositionLoc = -1;
    pointShadowLightRadiusLoc = -1;
    pointShadowHemisphereLoc = -1;
    pointShadowAlphaTestLoc = -1;
    pointShadowAlphaCutoffLoc = -1;
}

bool SectorDynamicLightingRenderer::IsShadowRenderReady() const
{
    return shadowMaterialLoaded
            && shadowMaterial.shader.id != 0
            && pointShadowMaterial.shader.id != 0
            && shadowLightViewProjectionLoc >= 0
            && !shadowMatrices.empty();
}

RenderTexture2D* SectorDynamicLightingRenderer::ShadowMap(std::size_t index)
{
    if (index != 0) {
        return nullptr;
    }
    return &shadowAtlas;
}

const RenderTexture2D* SectorDynamicLightingRenderer::ShadowMap(std::size_t index) const
{
    if (index != 0) {
        return nullptr;
    }
    return &shadowAtlas;
}

const Texture2D* SectorDynamicLightingRenderer::ShadowMapDepthTexture(std::size_t index) const
{
    const RenderTexture2D* shadowMap = ShadowMap(index);
    if (shadowMap == nullptr || shadowMap->depth.id == 0) {
        return nullptr;
    }
    return &shadowMap->depth;
}

SectorDynamicShadowMapTextures SectorDynamicLightingRenderer::BuildShadowMapTextures(
        bool enabled) const
{
    SectorDynamicShadowMapTextures textures;
    if (!enabled) {
        return textures;
    }
    textures.shadowAtlas = ShadowMapDepthTexture(0);
    textures.shadowMap0 = textures.shadowAtlas;
    textures.shadowMap1 = textures.shadowAtlas;
    return textures;
}

void SectorDynamicLightingRenderer::RenderShadowMaps(
        const SectorDynamicSpotLightShadowRenderContext& context)
{
    shadowRenderStats.doorCasterRevision = context.doorShadowCasterRevision;
    shadowRenderStats.staticModelCasterRevision =
            context.staticModelShadowCasterRevision;
    if (context.assets == nullptr
            || !IsShadowRenderReady()
            || context.sectorDrawRecords == nullptr
            || shadowMatrices.empty()) {
        return;
    }

    if (context.doorShadowCasterRevision
            != cachedDoorShadowCasterRevision) {
        shadowMapsCacheValid = false;
    }
    if (context.staticModelShadowCasterRevision
            != cachedStaticModelShadowCasterRevision) {
        shadowMapsCacheValid = false;
    }
    if (shadowMapsCacheValid) {
        shadowRenderStats.cacheHit = true;
        return;
    }
    bool cacheable = true;

    RenderTexture2D* shadowMap = ShadowMap(0);
    if (shadowMap == nullptr || shadowMap->id == 0 || shadowMap->depth.id == 0) {
        return;
    }
    BeginTextureMode(*shadowMap);
    ClearBackground(WHITE);
    shadowRenderStats.atlasRendered = true;
    rlEnableDepthTest();
    rlEnableScissorTest();

    for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix : shadowMatrices) {
        if (matrix.shadowSlot < 0) {
            continue;
        }

        if (matrix.dynamicLightIndex < 0
                || static_cast<std::size_t>(matrix.dynamicLightIndex) >= selectedLights.size()) {
            continue;
        }
        const int tilesPerRow = DynamicShadowAtlasResolution / shadowMapResolution;
        const int tileX = (matrix.shadowSlot % tilesPerRow) * shadowMapResolution;
        const int tileY = (matrix.shadowSlot / tilesPerRow) * shadowMapResolution;
        rlViewport(tileX, tileY, shadowMapResolution, shadowMapResolution);
        rlScissor(tileX, tileY, shadowMapResolution, shadowMapResolution);
        glClear(GL_DEPTH_BUFFER_BIT);

        const bool pointProjection = matrix.kind == SectorPreviewDynamicLightKind::Point;
        ++shadowRenderStats.renderedTiles;
        const SectorPreviewDynamicPointLightUniform& light =
                selectedLights[static_cast<std::size_t>(matrix.dynamicLightIndex)];
        Material& activeMaterial = pointProjection ? pointShadowMaterial : shadowMaterial;
        const int activeAlphaTestLoc = pointProjection
                ? pointShadowAlphaTestLoc : shadowAlphaTestLoc;
        const int activeAlphaCutoffLoc = pointProjection
                ? pointShadowAlphaCutoffLoc : shadowAlphaCutoffLoc;
        const Texture2D activeDefaultTexture = pointProjection
                ? pointShadowDefaultTexture : shadowDefaultTexture;
        if (pointProjection) {
            SetShaderValue(activeMaterial.shader, pointShadowLightPositionLoc,
                    &light.position, SHADER_UNIFORM_VEC3);
            SetShaderValue(activeMaterial.shader, pointShadowLightRadiusLoc,
                    &light.radius, SHADER_UNIFORM_FLOAT);
            SetShaderValue(activeMaterial.shader, pointShadowHemisphereLoc,
                    &matrix.pointHemisphere, SHADER_UNIFORM_INT);
        } else {
            SetShaderValueMatrix(activeMaterial.shader, shadowLightViewProjectionLoc,
                    matrix.lightViewProjection);
        }
        for (const SectorMeshBatch& batch : *context.sectorDrawRecords) {
            const SectorReceiverBounds* bounds = FindSectorReceiverBounds(
                    context.sectorReceiverBounds, batch.sectorId);
            if (bounds != nullptr
                    && !ShadowLightIntersectsBounds(
                            light, matrix, ToSectorAabb3(*bounds))) {
                ++shadowRenderStats.sectorBatchesCulled;
                continue;
            }
            ++shadowRenderStats.sectorBatchesDrawn;
            const int alphaTest = batch.alphaTest ? 1 : 0;
            const float alphaCutoff = batch.alphaCutoff;
            const Texture2D* texture = nullptr;
            if (batch.alphaTest && context.textureResolver != nullptr) {
                texture = context.textureResolver(context.userData, *context.assets, batch.textureId);
                if (texture == nullptr || texture->id == 0) {
                    cacheable = false;
                }
            }
            activeMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                    ? *texture
                    : activeDefaultTexture;
            if (activeAlphaTestLoc >= 0) {
                SetShaderValue(
                        activeMaterial.shader,
                        activeAlphaTestLoc,
                        &alphaTest,
                        SHADER_UNIFORM_INT);
            }
            if (activeAlphaCutoffLoc >= 0) {
                SetShaderValue(
                        activeMaterial.shader,
                        activeAlphaCutoffLoc,
                        &alphaCutoff,
                        SHADER_UNIFORM_FLOAT);
            }
            DrawMesh(batch.mesh, activeMaterial, MatrixIdentity());
        }
        const int doorAlphaTest = 0;
        const float doorAlphaCutoff = 0.0f;
        activeMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = activeDefaultTexture;
        if (activeAlphaTestLoc >= 0) {
            SetShaderValue(
                    activeMaterial.shader,
                    activeAlphaTestLoc,
                    &doorAlphaTest,
                    SHADER_UNIFORM_INT);
        }
        if (activeAlphaCutoffLoc >= 0) {
            SetShaderValue(
                    activeMaterial.shader,
                    activeAlphaCutoffLoc,
                    &doorAlphaCutoff,
                    SHADER_UNIFORM_FLOAT);
        }
        if (context.doorShadowCasters != nullptr && context.doorMeshResolver != nullptr) {
            for (const SectorDoorShadowCaster& caster : *context.doorShadowCasters) {
                if (!ShadowLightIntersectsBounds(
                            light, matrix, DoorCasterBounds(caster))) {
                    ++shadowRenderStats.objectCastersCulled;
                    continue;
                }
                float doorWidth = 0.0f;
                float doorHeight = 0.0f;
                const Mesh* doorMesh = context.doorMeshResolver(
                        context.doorMeshResolverUserData,
                        caster,
                        doorWidth,
                        doorHeight);
                if (doorMesh == nullptr || doorMesh->vertexCount <= 0) {
                    continue;
                }
                const Matrix shadowModel = BuildSectorDoorShadowCasterModelMatrix(
                        caster,
                        doorWidth,
                        doorHeight);
                DrawMesh(*doorMesh, activeMaterial, shadowModel);
                ++shadowRenderStats.objectCastersDrawn;
            }
        }
        if (context.doorModelShadowCasters != nullptr) {
            for (const SectorDoorModelShadowCaster& caster
                    : *context.doorModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    continue;
                }
                const Model& model = asset->model;
                const Matrix modelTransform = MatrixMultiply(
                        model.transform, caster.transform);
                const SectorAabb3 casterBounds = ToSectorAabb3(
                        TransformSectorDoorModelBounds(
                                asset->localBounds, modelTransform));
                if (!ShadowLightIntersectsBounds(
                            light, matrix, casterBounds)) {
                    ++shadowRenderStats.objectCastersCulled;
                    continue;
                }
                ++shadowRenderStats.objectCastersDrawn;
                for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
                    if (model.meshes[meshIndex].vertexCount <= 0) {
                        continue;
                    }
                    DrawMesh(
                            model.meshes[meshIndex],
                            activeMaterial,
                            modelTransform);
                }
            }
        }
        if (context.staticModelShadowCasters != nullptr) {
            rlDisableBackfaceCulling();
            for (const SectorStaticModelShadowCaster& caster
                    : *context.staticModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    if (!context.assets->HasFailed(caster.model)) {
                        cacheable = false;
                    }
                    continue;
                }
                const Model& model = asset->model;
                const Matrix modelTransform = MatrixMultiply(
                        model.transform, caster.transform);
                const SectorAabb3 casterBounds = ToSectorAabb3(
                        TransformSectorDoorModelBounds(
                                asset->localBounds, modelTransform));
                if (!ShadowLightIntersectsBounds(
                            light, matrix, casterBounds)) {
                    ++shadowRenderStats.objectCastersCulled;
                    continue;
                }
                ++shadowRenderStats.objectCastersDrawn;
                for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
                    if (model.meshes[meshIndex].vertexCount <= 0) {
                        continue;
                    }
                    DrawMesh(
                            model.meshes[meshIndex],
                            activeMaterial,
                            modelTransform);
                }
            }
            rlEnableBackfaceCulling();
        }
        activeMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = activeDefaultTexture;
    }
    rlDisableScissorTest();
    rlDisableDepthTest();
    EndTextureMode();
    shadowMapsCacheValid = cacheable;
    cachedDoorShadowCasterRevision = context.doorShadowCasterRevision;
    cachedStaticModelShadowCasterRevision =
            context.staticModelShadowCasterRevision;
}

void SectorDynamicLightingRenderer::ReserveSelectionBuffers()
{
    candidates.clear();
    candidates.reserve(sources.size() + 1);
    selectedLights.clear();
    selectedLights.reserve(MaxDynamicLights);
    selectedLightIds.clear();
    selectedLightIds.reserve(MaxDynamicLights);
    shadowCasters.clear();
    shadowCasters.reserve(MaxDynamicSpotLightShadowCasters);
    shadowMatrices.clear();
    shadowMatrices.reserve(MaxDynamicSpotLightShadowCasters);
    cachedShadowMatrices.clear();
    cachedShadowMatrices.reserve(MaxDynamicSpotLightShadowCasters);
}

void SectorDynamicLightingRenderer::BuildReceiverBounds(
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld)
{
    receiverBounds.clear();
    receiverBounds.insert(receiverBounds.end(), sectorReceiverBounds.begin(), sectorReceiverBounds.end());
    if (runtimeObjectWorld != nullptr) {
        CollectSectorDoorReceiverBounds(*runtimeObjectWorld, receiverBounds);
    }
}

} // namespace game
