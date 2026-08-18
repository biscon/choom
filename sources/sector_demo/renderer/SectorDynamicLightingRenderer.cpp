#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorStaticModelShadow.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <external/glad.h>
#include <raymath.h>
#include <rlgl.h>

namespace game {

namespace {

bool ShadowMatrixMatches(
        const SectorPreviewDynamicSpotLightShadowMatrix& left,
        const SectorPreviewDynamicSpotLightShadowMatrix& right)
{
    return left.lightId == right.lightId
            && left.shadowSlot == right.shadowSlot
            && left.kind == right.kind
            && left.pointHemisphere == right.pointHemisphere
            && std::memcmp(&left.lightPosition,
                    &right.lightPosition, sizeof(Vector3)) == 0
            && left.lightRadius == right.lightRadius
            && std::memcmp(&left.lightViewProjection,
                    &right.lightViewProjection, sizeof(Matrix)) == 0;
}

bool DynamicPortalBlockerMatches(
        const RuntimePortalDynamicBlocker& left,
        const RuntimePortalDynamicBlocker& right)
{
    return left.lineDefId == right.lineDefId
            && left.sideDefId == right.sideDefId
            && left.fromSectorId == right.fromSectorId
            && left.toSectorId == right.toSectorId
            && left.blocksPortal == right.blocksPortal;
}

bool DynamicPortalBlockersMatch(
        const std::vector<RuntimePortalDynamicBlocker>& cached,
        const std::vector<RuntimePortalDynamicBlocker>* current)
{
    const std::size_t currentSize = current != nullptr ? current->size() : 0;
    if (cached.size() != currentSize) return false;
    for (const RuntimePortalDynamicBlocker& cachedBlocker : cached) {
        const auto found = std::find_if(
                current->begin(),
                current->end(),
                [&cachedBlocker](const RuntimePortalDynamicBlocker& currentBlocker) {
                    return DynamicPortalBlockerMatches(
                            cachedBlocker,
                            currentBlocker);
                });
        if (found == current->end()) return false;
    }
    return true;
}

bool LightingStartSectorsMatch(
        const std::vector<int>& cached,
        int lightingStartSectorId)
{
    return lightingStartSectorId > 0
            ? cached.size() == 1 && cached[0] == lightingStartSectorId
            : cached.empty();
}

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

const char* SectorSpotLightShadowOpaqueFs = R"(
#version 330
void main() {}
)";

const char* SectorSpotLightShadowCutoutFs = R"(
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
    std::array<Vector3, MaxDynamicLights> spotShadowRight{};
    std::array<Vector2, MaxDynamicLights> spotShadowProjection{};
    for (int i = 0; i < lightCount; ++i) {
        positions[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].position;
        colors[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].color;
        radii[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].radius;
        intensities[static_cast<size_t>(i)] = DynamicLightEffectiveUploadIntensity(
                lights[static_cast<size_t>(i)],
                runtimeSeconds);
        types[static_cast<size_t>(i)] = static_cast<int>(lights[static_cast<size_t>(i)].kind);
        directions[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].kind
                        == SectorPreviewDynamicLightKind::Spot
                ? (Vector3LengthSqr(lights[static_cast<size_t>(i)].direction)
                                > 0.00000001f
                        ? Vector3Normalize(lights[static_cast<size_t>(i)].direction)
                        : Vector3{0.0f, -1.0f, 0.0f})
                : lights[static_cast<size_t>(i)].direction;
        innerConeCos[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].innerConeCos;
        outerConeCos[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].outerConeCos;
        if (lights[static_cast<size_t>(i)].kind
                == SectorPreviewDynamicLightKind::Spot) {
            BuildSectorDynamicSpotShadowProjectionUpload(
                    lights[static_cast<size_t>(i)],
                    spotShadowRight[static_cast<size_t>(i)],
                    spotShadowProjection[static_cast<size_t>(i)]);
        } else if (lights[static_cast<size_t>(i)].castsShadow) {
            context.hasPointShadows = 1;
        }
    }

    context.dynamicLightPositions = positions;
    context.dynamicLightColors = colors;
    context.dynamicLightRadii = radii;
    context.dynamicLightIntensities = intensities;
    context.dynamicLightTypes = types;
    context.dynamicLightDirections = directions;
    context.dynamicLightInnerConeCos = innerConeCos;
    context.dynamicLightOuterConeCos = outerConeCos;
    context.dynamicLightSpotShadowRight = spotShadowRight;
    context.dynamicLightSpotShadowProjection = spotShadowProjection;
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
    if (locations.hasPointShadows >= 0) {
        SetShaderValue(
                shader,
                locations.hasPointShadows,
                &context.hasPointShadows,
                SHADER_UNIFORM_INT);
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
    if (locations.dynamicLightSpotShadowRight >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightSpotShadowRight,
                context.dynamicLightSpotShadowRight.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightSpotShadowProjection >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightSpotShadowProjection,
                context.dynamicLightSpotShadowProjection.data(),
                SHADER_UNIFORM_VEC2,
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
    selectedLightKeys.clear();
    lightingVisibility = {};
    cachedLightingStartSectorIds.clear();
    cachedLightingPortalBlockers.clear();
    cachedLightingVisibilityGraph = nullptr;
    lightingVisibilityCacheValid = false;
    selectionStats = {};
    receiverBounds.clear();
    sectorLightContexts.clear();
    shadowCasters.clear();
    shadowMatrices.clear();
    for (ShadowAtlasTileState& state : shadowAtlasTileStates) {
        state = {};
    }
    for (SectorDynamicShadowSlotOwner& owner : shadowAtlasSlotOwners) {
        owner = {};
    }
    pendingShadowLightUpdates.clear();
    previousDoorShadowCasterBounds.clear();
    currentDoorShadowCasterBounds.clear();
    previousStaticShadowCasterBounds.clear();
    currentStaticShadowCasterBounds.clear();
    changedShadowCasterBounds.clear();
    nextShadowDirtySerial = 1;
    shadowAtlasNeedsFullClear = true;
    doorShadowCasterBoundsInitialized = false;
    staticShadowCasterBoundsInitialized = false;
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
    lightingVisibilityCacheValid = false;
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
    previousDoorShadowCasterBounds.reserve(runtimeObjectCapacity);
    currentDoorShadowCasterBounds.reserve(runtimeObjectCapacity);
    previousStaticShadowCasterBounds.reserve(runtimeObjectCapacity);
    currentStaticShadowCasterBounds.reserve(runtimeObjectCapacity);
    changedShadowCasterBounds.reserve(runtimeObjectCapacity * 4);
    cachedLightingPortalBlockers.reserve(runtimeObjectCapacity * 2);
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
        int lightingStartSectorId,
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld,
        const RuntimeSectorVisibilityGraph* visibilityGraph,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers)
{
    BuildReceiverBounds(sectorReceiverBounds, runtimeObjectWorld);
    UpdateLightingReachability(
            visibility,
            lightingStartSectorId,
            visibilityGraph,
            dynamicPortalBlockers);
    selectionSources.assign(sources.begin(), sources.end());
    if (runtimePointLightActive) selectionSources.push_back(runtimePointLight);
    CollectSectorPreviewDynamicPointLightCandidates(
            selectionSources,
            lightingVisibility,
            receiverBounds,
            candidates);
    SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            lightingVisibility,
            receiverBounds,
            maxDynamicLights,
            selectedLights,
            &selectedLightKeys,
            &selectedLightKeys);
    SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selectedLights,
            lightingVisibility,
            receiverBounds,
            ShadowSlotBudget(),
            shadowCasters);
    AssignPersistentSectorDynamicShadowSlots(
            selectedLights,
            ShadowSlotBudget(),
            shadowCasters,
            shadowAtlasSlotOwners);
    BuildSectorPreviewDynamicSpotLightShadowMatrices(
            selectedLights,
            shadowCasters,
            shadowMatrices);
    RefreshShadowTileRequirements();
    UpdateSelectionStats(visibility);
}

void SectorDynamicLightingRenderer::UpdateLightingReachability(
        const RuntimePortalVisibilityResult& visibility,
        int lightingStartSectorId,
        const RuntimeSectorVisibilityGraph* visibilityGraph,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers)
{
    selectionStats.reachabilityCacheHit = false;
    selectionStats.cameraVisibilityFallback = visibility.fallbackDrawAll;
    selectionStats.dynamicPortalBlockerCount = dynamicPortalBlockers != nullptr
            ? dynamicPortalBlockers->size()
            : 0;

    int resolvedStartSectorId = lightingStartSectorId;
    if (visibilityGraph != nullptr
            && FindRuntimeSectorVisibilityNode(
                    *visibilityGraph,
                    resolvedStartSectorId) == nullptr) {
        resolvedStartSectorId = visibility.validStartSector
                ? visibility.startSectorId
                : -1;
    }
    selectionStats.lightingStartSectorId = resolvedStartSectorId;

    if (visibilityGraph == nullptr
            || FindRuntimeSectorVisibilityNode(
                    *visibilityGraph,
                    resolvedStartSectorId) == nullptr) {
        lightingVisibility = {};
        lightingVisibility.totalSectorCount = visibilityGraph != nullptr
                ? visibilityGraph->sectors.size()
                : visibility.totalSectorCount;
        lightingVisibility.fallbackDrawAll = true;
        lightingVisibility.status = "lighting reachability unavailable; fallback all";
        lightingVisibilityCacheValid = false;
        return;
    }

    if (lightingVisibilityCacheValid
            && cachedLightingVisibilityGraph == visibilityGraph
            && LightingStartSectorsMatch(
                    cachedLightingStartSectorIds,
                    resolvedStartSectorId)
            && DynamicPortalBlockersMatch(
                    cachedLightingPortalBlockers,
                    dynamicPortalBlockers)) {
        selectionStats.reachabilityCacheHit = true;
        return;
    }

    if (resolvedStartSectorId > 0) {
        cachedLightingStartSectorIds.assign(1, resolvedStartSectorId);
    } else {
        cachedLightingStartSectorIds.clear();
    }
    if (dynamicPortalBlockers != nullptr) {
        cachedLightingPortalBlockers = *dynamicPortalBlockers;
    } else {
        cachedLightingPortalBlockers.clear();
    }
    cachedLightingVisibilityGraph = visibilityGraph;
    lightingVisibility = TraverseRuntimeSectorVisibilityFromSeeds(
            *visibilityGraph,
            cachedLightingStartSectorIds,
            resolvedStartSectorId,
            dynamicPortalBlockers);
    lightingVisibilityCacheValid = true;
}

void SectorDynamicLightingRenderer::UpdateSelectionStats(
        const RuntimePortalVisibilityResult& cameraVisibility)
{
    selectionStats.reachableSectorCount = lightingVisibility.fallbackDrawAll
            ? lightingVisibility.totalSectorCount
            : lightingVisibility.visibleSectorIds.size();
    selectionStats.visibleReceiverCount = 0;
    selectionStats.visibleReceiverLightReferences = 0;
    selectionStats.maxVisibleReceiverLights = 0;

    for (const SectorReceiverBounds& bounds : receiverBounds) {
        if (!IsValidSectorAabb3(SectorAabb3{bounds.min, bounds.max})) continue;
        if (!ShouldDrawRuntimeSectorForVisibility(bounds.sectorId, cameraVisibility)) {
            continue;
        }

        ++selectionStats.visibleReceiverCount;
        std::size_t lightCount = 0;
        const SectorAabb3 receiverAabb = ToSectorAabb3(bounds);
        for (const SectorPreviewDynamicPointLightUniform& light : selectedLights) {
            if (DynamicLightIntersectsBounds(light, receiverAabb)) ++lightCount;
        }
        selectionStats.visibleReceiverLightReferences += lightCount;
        selectionStats.maxVisibleReceiverLights = std::max(
                selectionStats.maxVisibleReceiverLights,
                lightCount);
    }
}

void SectorDynamicLightingRenderer::RefreshShadowTileRequirements()
{
    for (ShadowAtlasTileState& state : shadowAtlasTileStates) {
        state.assigned = false;
    }

    for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix : shadowMatrices) {
        if (matrix.shadowSlot < 0
                || static_cast<std::size_t>(matrix.shadowSlot)
                        >= shadowAtlasTileStates.size()) {
            continue;
        }
        ShadowAtlasTileState& state =
                shadowAtlasTileStates[static_cast<std::size_t>(matrix.shadowSlot)];
        const bool compatible = state.valid
                && ShadowMatrixMatches(state.matrix, matrix);
        state.assigned = true;
        state.matrix = matrix;
        if (!compatible) {
            state.valid = false;
            if (!state.dirty) {
                state.dirtySerial = nextShadowDirtySerial++;
            }
            state.dirty = true;
            if (state.dirtySerial == 0) {
                state.dirtySerial = nextShadowDirtySerial++;
            }
        }
    }

    // Both point-light hemispheres form one cache entry and must always be
    // rebuilt together. Give both slots the oldest serial in the pair.
    for (const SectorPreviewDynamicSpotLightShadowCaster& caster : shadowCasters) {
        if (caster.shadowSlot < 0 || caster.shadowSlotCount <= 0) continue;
        uint64_t serial = 0;
        bool dirty = false;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            const ShadowAtlasTileState& state = shadowAtlasTileStates[slot];
            dirty = dirty || state.dirty;
            if (state.dirtySerial != 0
                    && (serial == 0 || state.dirtySerial < serial)) {
                serial = state.dirtySerial;
            }
        }
        if (!dirty) continue;
        if (serial == 0) serial = nextShadowDirtySerial++;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            shadowAtlasTileStates[slot].dirty = true;
            shadowAtlasTileStates[slot].dirtySerial = serial;
        }
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
        context.dynamicLightDirections[localIndex] = light.kind
                        == SectorPreviewDynamicLightKind::Spot
                ? (Vector3LengthSqr(light.direction) > 0.00000001f
                        ? Vector3Normalize(light.direction)
                        : Vector3{0.0f, -1.0f, 0.0f})
                : light.direction;
        context.dynamicLightInnerConeCos[localIndex] = light.innerConeCos;
        context.dynamicLightOuterConeCos[localIndex] = light.outerConeCos;
        if (light.kind == SectorPreviewDynamicLightKind::Spot) {
            BuildSectorDynamicSpotShadowProjectionUpload(
                    light,
                    context.dynamicLightSpotShadowRight[localIndex],
                    context.dynamicLightSpotShadowProjection[localIndex]);
        }
        const int shadowSlot = globalShadowSlots[selectedIndex];
        context.shadowUniforms.dynamicLightShadowSlots[localIndex] = shadowSlot;
        if (light.kind == SectorPreviewDynamicLightKind::Point
                && shadowSlot >= 0
                && static_cast<std::size_t>(shadowSlot)
                        < context.shadowUniforms.shadowStrength.size()
                && context.shadowUniforms.shadowStrength[
                        static_cast<std::size_t>(shadowSlot)] > 0.0f) {
            context.hasPointShadows = 1;
        }
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
    for (std::size_t lightIndex = 0;
            lightIndex < selectedLights.size()
                    && lightIndex < result.dynamicLightShadowSlots.size();
            ++lightIndex) {
        const int slot = result.dynamicLightShadowSlots[lightIndex];
        if (slot < 0
                || static_cast<std::size_t>(slot) >= shadowAtlasTileStates.size()) {
            continue;
        }
        const ShadowAtlasTileState& state =
                shadowAtlasTileStates[static_cast<std::size_t>(slot)];
        const int requiredSlots = selectedLights[lightIndex].kind
                        == SectorPreviewDynamicLightKind::Point
                ? 2 : 1;
        bool valid = state.assigned && state.valid;
        for (int offset = 1; valid && offset < requiredSlots; ++offset) {
            const std::size_t adjacent = static_cast<std::size_t>(slot + offset);
            valid = adjacent < shadowAtlasTileStates.size()
                    && shadowAtlasTileStates[adjacent].assigned
                    && shadowAtlasTileStates[adjacent].valid;
        }
        if (!valid) result.dynamicLightShadowSlots[lightIndex] = -1;
    }
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
    shadowAtlasNeedsFullClear = true;
    return true;
}

void SectorDynamicLightingRenderer::SetShadowMapResolution(int resolution)
{
    resolution = std::clamp(resolution, 256, 2048);
    if (shadowMapResolution == resolution) {
        return;
    }
    shadowMapResolution = resolution;
    for (ShadowAtlasTileState& state : shadowAtlasTileStates) state = {};
    for (SectorDynamicShadowSlotOwner& owner : shadowAtlasSlotOwners) owner = {};
    shadowAtlasNeedsFullClear = true;
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
    Shader shader = LoadShaderFromMemory(
            SectorSpotLightShadowVs, SectorSpotLightShadowOpaqueFs);
    if (shader.id == 0) {
        UnloadMaterial(shadowMaterial);
        shadowMaterial = Material{};
        return false;
    }
    spotShadowCutoutMaterial = LoadMaterialDefault();
    Shader spotCutoutShader = LoadShaderFromMemory(
            SectorSpotLightShadowVs, SectorSpotLightShadowCutoutFs);
    if (spotCutoutShader.id == 0) {
        UnloadShader(shader);
        UnloadMaterial(shadowMaterial);
        UnloadMaterial(spotShadowCutoutMaterial);
        shadowMaterial = Material{};
        spotShadowCutoutMaterial = Material{};
        return false;
    }
    pointShadowMaterial = LoadMaterialDefault();
    Shader pointShader = LoadGeometryShader(
            SectorPointLightShadowVs,
            SectorPointLightShadowGs,
            SectorPointLightShadowFs);
    if (pointShader.id == 0) {
        UnloadShader(shader);
        UnloadShader(spotCutoutShader);
        UnloadMaterial(shadowMaterial);
        UnloadMaterial(spotShadowCutoutMaterial);
        UnloadMaterial(pointShadowMaterial);
        shadowMaterial = Material{};
        spotShadowCutoutMaterial = Material{};
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
    shadowDefaultTexture = shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    spotShadowCutoutMaterial.shader = spotCutoutShader;
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(spotCutoutShader, "vertexPosition");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(spotCutoutShader, "vertexTexCoord");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(spotCutoutShader, "matModel");
    spotShadowCutoutMaterial.shader.locs[SHADER_LOC_MAP_DIFFUSE] =
            GetShaderLocation(spotCutoutShader, "texture0");
    spotShadowCutoutLightViewProjectionLoc =
            GetShaderLocation(spotCutoutShader, "lightViewProjection");
    shadowAlphaTestLoc = GetShaderLocation(spotCutoutShader, "alphaTest");
    shadowAlphaCutoffLoc = GetShaderLocation(spotCutoutShader, "alphaCutoff");
    spotShadowCutoutDefaultTexture =
            spotShadowCutoutMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
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
    spotShadowCutoutMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
            spotShadowCutoutDefaultTexture;
    pointShadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = pointShadowDefaultTexture;
    UnloadMaterial(shadowMaterial);
    UnloadMaterial(spotShadowCutoutMaterial);
    UnloadMaterial(pointShadowMaterial);
    shadowMaterial = Material{};
    spotShadowCutoutMaterial = Material{};
    pointShadowMaterial = Material{};
    shadowDefaultTexture = Texture2D{};
    spotShadowCutoutDefaultTexture = Texture2D{};
    pointShadowDefaultTexture = Texture2D{};
    shadowMaterialLoaded = false;
    shadowLightViewProjectionLoc = -1;
    spotShadowCutoutLightViewProjectionLoc = -1;
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
            && spotShadowCutoutMaterial.shader.id != 0
            && pointShadowMaterial.shader.id != 0
            && shadowLightViewProjectionLoc >= 0
            && spotShadowCutoutLightViewProjectionLoc >= 0
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
    const auto cpuStart = std::chrono::steady_clock::now();
    const auto finishCpuTiming = [this, cpuStart]() {
        shadowRenderStats.cpuMilliseconds =
                std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - cpuStart).count();
    };
    shadowRenderStats.doorCasterRevision = context.doorShadowCasterRevision;
    shadowRenderStats.staticModelCasterRevision =
            context.staticModelShadowCasterRevision;
    if (context.assets == nullptr
            || !IsShadowRenderReady()
            || context.sectorDrawRecords == nullptr
            || shadowMatrices.empty()) {
        return;
    }

    RenderTexture2D* shadowMap = ShadowMap(0);
    if (shadowMap == nullptr || shadowMap->id == 0 || shadowMap->depth.id == 0) {
        return;
    }

    if (shadowAtlasNeedsFullClear) {
        for (ShadowAtlasTileState& state : shadowAtlasTileStates) {
            if (!state.assigned) continue;
            state.valid = false;
            state.dirty = true;
            if (state.dirtySerial == 0) {
                state.dirtySerial = nextShadowDirtySerial++;
            }
        }
    }

    const bool doorRevisionChanged = context.doorShadowCasterRevision
            != cachedDoorShadowCasterRevision;
    const bool staticRevisionChanged = context.staticModelShadowCasterRevision
            != cachedStaticModelShadowCasterRevision;
    const bool refreshDoorBounds = doorRevisionChanged
            || !doorShadowCasterBoundsInitialized;
    const bool refreshStaticBounds = staticRevisionChanged
            || !staticShadowCasterBoundsInitialized;
    changedShadowCasterBounds.clear();
    bool preciseCasterChanges = true;
    const auto appendChangedBounds = [this](
            const std::vector<ShadowCasterBoundsRecord>& current,
            const std::vector<ShadowCasterBoundsRecord>& previous) {
        for (const ShadowCasterBoundsRecord& currentRecord : current) {
            const auto previousIt = std::find_if(
                    previous.begin(), previous.end(),
                    [&currentRecord](const ShadowCasterBoundsRecord& candidate) {
                        return candidate.key == currentRecord.key;
                    });
            if (previousIt == previous.end()) {
                changedShadowCasterBounds.push_back(currentRecord.bounds);
                continue;
            }
            if (std::memcmp(&previousIt->bounds,
                        &currentRecord.bounds, sizeof(BoundingBox)) != 0) {
                changedShadowCasterBounds.push_back(previousIt->bounds);
                changedShadowCasterBounds.push_back(currentRecord.bounds);
            }
        }
        for (const ShadowCasterBoundsRecord& previousRecord : previous) {
            const auto currentIt = std::find_if(
                    current.begin(), current.end(),
                    [&previousRecord](const ShadowCasterBoundsRecord& candidate) {
                        return candidate.key == previousRecord.key;
                    });
            if (currentIt == current.end()) {
                changedShadowCasterBounds.push_back(previousRecord.bounds);
            }
        }
    };
    if (refreshDoorBounds) {
        currentDoorShadowCasterBounds.clear();
        if (context.doorShadowCasters != nullptr) {
            for (const SectorDoorShadowCaster& caster : *context.doorShadowCasters) {
                const SectorAabb3 bounds = DoorCasterBounds(caster);
                currentDoorShadowCasterBounds.push_back(ShadowCasterBoundsRecord{
                        (uint64_t{1} << 32)
                                | static_cast<uint32_t>(caster.placedObjectId),
                        BoundingBox{bounds.min, bounds.max}});
            }
        }
        if (context.doorModelShadowCasters != nullptr) {
            for (const SectorDoorModelShadowCaster& caster
                    : *context.doorModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    preciseCasterChanges = false;
                    continue;
                }
                const Matrix transform = MatrixMultiply(
                        asset->model.transform, caster.transform);
                currentDoorShadowCasterBounds.push_back(ShadowCasterBoundsRecord{
                        (uint64_t{2} << 32)
                                | static_cast<uint32_t>(caster.placedObjectId),
                        TransformSectorDoorModelBounds(
                                asset->localBounds, transform)});
            }
        }
        appendChangedBounds(
                currentDoorShadowCasterBounds,
                previousDoorShadowCasterBounds);
        previousDoorShadowCasterBounds.swap(currentDoorShadowCasterBounds);
        doorShadowCasterBoundsInitialized = true;
    }
    if (refreshStaticBounds) {
        currentStaticShadowCasterBounds.clear();
        if (context.staticModelShadowCasters != nullptr) {
            for (const SectorStaticModelShadowCaster& caster
                    : *context.staticModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    preciseCasterChanges = false;
                    continue;
                }
                const Matrix transform = MatrixMultiply(
                        asset->model.transform, caster.transform);
                currentStaticShadowCasterBounds.push_back(ShadowCasterBoundsRecord{
                        (uint64_t{3} << 32)
                                | static_cast<uint32_t>(caster.placedObjectId),
                        TransformSectorDoorModelBounds(
                                asset->localBounds, transform)});
            }
        }
        appendChangedBounds(
                currentStaticShadowCasterBounds,
                previousStaticShadowCasterBounds);
        previousStaticShadowCasterBounds.swap(currentStaticShadowCasterBounds);
        staticShadowCasterBoundsInitialized = true;
    }

    const auto markCasterDirty = [this](
            const SectorPreviewDynamicSpotLightShadowCaster& caster) {
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            ShadowAtlasTileState& state = shadowAtlasTileStates[slot];
            if (!state.dirty) state.dirtySerial = nextShadowDirtySerial++;
            state.dirty = true;
        }
    };
    if (refreshDoorBounds || refreshStaticBounds) {
        for (const SectorPreviewDynamicSpotLightShadowCaster& caster
                : shadowCasters) {
            bool affected = !preciseCasterChanges;
            if (!affected && caster.dynamicLightIndex >= 0
                    && static_cast<std::size_t>(caster.dynamicLightIndex)
                            < selectedLights.size()) {
                const SectorPreviewDynamicPointLightUniform& light =
                        selectedLights[static_cast<std::size_t>(
                                caster.dynamicLightIndex)];
                for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix
                        : shadowMatrices) {
                    if (matrix.lightId != caster.lightId
                            || matrix.dynamicLightIndex != caster.dynamicLightIndex) {
                        continue;
                    }
                    for (const BoundingBox& changedBounds
                            : changedShadowCasterBounds) {
                        if (ShadowLightIntersectsBounds(
                                    light, matrix, ToSectorAabb3(changedBounds))) {
                            affected = true;
                            break;
                        }
                    }
                    if (affected) break;
                }
            }
            if (affected) markCasterDirty(caster);
        }
    }
    cachedDoorShadowCasterRevision = context.doorShadowCasterRevision;
    cachedStaticModelShadowCasterRevision =
            context.staticModelShadowCasterRevision;

    pendingShadowLightUpdates.clear();
    for (std::size_t casterIndex = 0;
            casterIndex < shadowCasters.size(); ++casterIndex) {
        const SectorPreviewDynamicSpotLightShadowCaster& caster =
                shadowCasters[casterIndex];
        if (caster.shadowSlot < 0 || caster.shadowSlotCount <= 0) continue;
        bool dirty = false;
        bool invalid = false;
        bool valid = true;
        uint64_t serial = 0;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            const ShadowAtlasTileState& state = shadowAtlasTileStates[slot];
            dirty = dirty || state.dirty;
            invalid = invalid || !state.valid;
            valid = valid && state.valid;
            if (state.dirtySerial != 0
                    && (serial == 0 || state.dirtySerial < serial)) {
                serial = state.dirtySerial;
            }
        }
        if (valid) ++shadowRenderStats.validLights;
        shadowRenderStats.occupiedTiles +=
                static_cast<std::size_t>(caster.shadowSlotCount);
        if (caster.shadowSlotCount == 2) ++shadowRenderStats.pointLights;
        else ++shadowRenderStats.spotLights;
        if (!dirty) continue;
        ++shadowRenderStats.dirtyLights;
        pendingShadowLightUpdates.push_back(SectorDynamicShadowUpdateRequest{
                casterIndex, invalid, serial, caster.shadowSlotCount});
    }
    SortSectorDynamicShadowUpdateRequests(pendingShadowLightUpdates);
    shadowRenderStats.queuedLights = pendingShadowLightUpdates.size();
    if (pendingShadowLightUpdates.empty()) {
        shadowRenderStats.cacheHit = true;
        finishCpuTiming();
        return;
    }

    const std::size_t updateCount = SectorDynamicShadowUpdateCount(
            pendingShadowLightUpdates.size(), maxShadowLightUpdatesPerFrame);
    BeginTextureMode(*shadowMap);
    const bool fullClear = shadowAtlasNeedsFullClear
            || (updateCount == pendingShadowLightUpdates.size()
                    && pendingShadowLightUpdates.size() == shadowCasters.size());
    if (fullClear) {
        ClearBackground(WHITE);
        shadowAtlasNeedsFullClear = false;
    }
    shadowRenderStats.atlasRendered = true;
    rlEnableDepthTest();
    rlEnableScissorTest();

    for (std::size_t updateIndex = 0; updateIndex < updateCount; ++updateIndex) {
        const SectorPreviewDynamicSpotLightShadowCaster& updateCaster =
                shadowCasters[pendingShadowLightUpdates[updateIndex].casterIndex];
        bool lightCacheable = true;
        for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix : shadowMatrices) {
            if (matrix.lightId != updateCaster.lightId
                    || matrix.dynamicLightIndex != updateCaster.dynamicLightIndex
                    || matrix.shadowSlot < updateCaster.shadowSlot
                    || matrix.shadowSlot >= updateCaster.shadowSlot
                            + updateCaster.shadowSlotCount) {
                continue;
            }
            if (matrix.shadowSlot < 0) {
                continue;
            }

            if (matrix.dynamicLightIndex < 0
                    || static_cast<std::size_t>(matrix.dynamicLightIndex)
                            >= selectedLights.size()) {
                continue;
            }
        const int tilesPerRow = DynamicShadowAtlasResolution / shadowMapResolution;
        const int tileX = (matrix.shadowSlot % tilesPerRow) * shadowMapResolution;
        const int tileY = (matrix.shadowSlot / tilesPerRow) * shadowMapResolution;
        rlViewport(tileX, tileY, shadowMapResolution, shadowMapResolution);
        rlScissor(tileX, tileY, shadowMapResolution, shadowMapResolution);
        if (!fullClear) glClear(GL_DEPTH_BUFFER_BIT);

        const bool pointProjection = matrix.kind == SectorPreviewDynamicLightKind::Point;
        ++shadowRenderStats.renderedTiles;
        const SectorPreviewDynamicPointLightUniform& light =
                selectedLights[static_cast<std::size_t>(matrix.dynamicLightIndex)];
        Material& activeMaterial = pointProjection ? pointShadowMaterial : shadowMaterial;
        const int activeAlphaTestLoc = pointProjection
                ? pointShadowAlphaTestLoc : -1;
        const int activeAlphaCutoffLoc = pointProjection
                ? pointShadowAlphaCutoffLoc : -1;
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
            SetShaderValueMatrix(
                    spotShadowCutoutMaterial.shader,
                    spotShadowCutoutLightViewProjectionLoc,
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
                    lightCacheable = false;
                }
            }
            Material& batchMaterial = !pointProjection && batch.alphaTest
                    ? spotShadowCutoutMaterial : activeMaterial;
            const int batchAlphaTestLoc = !pointProjection && batch.alphaTest
                    ? shadowAlphaTestLoc : activeAlphaTestLoc;
            const int batchAlphaCutoffLoc = !pointProjection && batch.alphaTest
                    ? shadowAlphaCutoffLoc : activeAlphaCutoffLoc;
            const Texture2D batchDefaultTexture = !pointProjection && batch.alphaTest
                    ? spotShadowCutoutDefaultTexture : activeDefaultTexture;
            batchMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                    ? *texture
                    : batchDefaultTexture;
            if (batchAlphaTestLoc >= 0) {
                SetShaderValue(
                        batchMaterial.shader,
                        batchAlphaTestLoc,
                        &alphaTest,
                        SHADER_UNIFORM_INT);
            }
            if (batchAlphaCutoffLoc >= 0) {
                SetShaderValue(
                        batchMaterial.shader,
                        batchAlphaCutoffLoc,
                        &alphaCutoff,
                        SHADER_UNIFORM_FLOAT);
            }
            DrawMesh(batch.mesh, batchMaterial, MatrixIdentity());
        }
        const int doorAlphaTest = 0;
        const float doorAlphaCutoff = 0.0f;
        activeMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = activeDefaultTexture;
        spotShadowCutoutMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
                spotShadowCutoutDefaultTexture;
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
                        lightCacheable = false;
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
        for (int offset = 0; offset < updateCaster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    updateCaster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) continue;
            ShadowAtlasTileState& state = shadowAtlasTileStates[slot];
            state.valid = lightCacheable;
            state.dirty = !lightCacheable;
            state.dirtySerial = lightCacheable ? 0 : state.dirtySerial;
        }
        ++shadowRenderStats.updatedLights;
    }
    rlDisableScissorTest();
    rlDisableDepthTest();
    EndTextureMode();
    shadowRenderStats.validLights = 0;
    shadowRenderStats.dirtyLights = 0;
    for (const SectorPreviewDynamicSpotLightShadowCaster& caster
            : shadowCasters) {
        bool valid = true;
        bool dirty = false;
        for (int offset = 0; offset < caster.shadowSlotCount; ++offset) {
            const std::size_t slot = static_cast<std::size_t>(
                    caster.shadowSlot + offset);
            if (slot >= shadowAtlasTileStates.size()) {
                valid = false;
                continue;
            }
            valid = valid && shadowAtlasTileStates[slot].valid;
            dirty = dirty || shadowAtlasTileStates[slot].dirty;
        }
        if (valid) ++shadowRenderStats.validLights;
        if (dirty) ++shadowRenderStats.dirtyLights;
    }
    shadowRenderStats.queuedLights = shadowRenderStats.dirtyLights;
    finishCpuTiming();
}

void SectorDynamicLightingRenderer::ReserveSelectionBuffers()
{
    candidates.clear();
    candidates.reserve(sources.size() + 1);
    selectedLights.clear();
    selectedLights.reserve(MaxDynamicLights);
    selectedLightKeys.clear();
    selectedLightKeys.reserve(MaxDynamicLights);
    cachedLightingStartSectorIds.reserve(6);
    cachedLightingPortalBlockers.reserve(16);
    shadowCasters.clear();
    shadowCasters.reserve(MaxDynamicSpotLightShadowCasters);
    shadowMatrices.clear();
    shadowMatrices.reserve(MaxDynamicSpotLightShadowCasters);
    pendingShadowLightUpdates.clear();
    pendingShadowLightUpdates.reserve(MaxDynamicLights);
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
