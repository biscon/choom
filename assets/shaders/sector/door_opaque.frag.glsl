#version 330
#include "reflection_sampling.glsl"
in vec2 fragTexCoord;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D normalTexture;
uniform sampler2D materialPropertiesTexture;
uniform int hasNormalMap;
uniform float normalStrength;
uniform int materialPropertiesKind;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3 cameraPosition;
uniform samplerCube environmentTexture;
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
uniform int pbrDiagnosticMode;
uniform int useObjectAmbientCube;
uniform vec3 objectAmbientCube[6];

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
uniform int useStaticSpecularLighting;

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
uniform vec3 dynamicLightSpotShadowRight[MAX_DYNAMIC_LIGHTS];
uniform vec2 dynamicLightSpotShadowProjection[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightProfiles[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightProfileParameters[MAX_DYNAMIC_LIGHTS];
uniform sampler2D flashlightCookie;
uniform int hasPointShadows;
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];

#define MAX_DYNAMIC_SHADOW_CASTERS 64
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform int shadowAtlasTilesPerRow;
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

uniform vec4 doorTint;

uniform int fogEnabled;
uniform vec3 fogColor;
uniform vec3 fogCameraPosition;
uniform float fogStartDistanceWorld;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeightWorld;
uniform float fogHeightFalloff;

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

#include "safe_normalize.glsl"


#include "flashlight_profile.glsl"

#include "finite_half_radiance.glsl"

#include "pbr.glsl"



#include "dynamic_shadow.glsl"

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

#include "surface_fog.glsl"

void main()
{
    vec3 geometricNormal = SafeNormalize(
            fragWorldNormal, vec3(0.0, 1.0, 0.0));
    vec3 receiverPlaneNormal = geometricNormal;
    if (hasPointShadows != 0) {
        receiverPlaneNormal = SafeNormalize(
                cross(dFdx(fragWorldPosition), dFdy(fragWorldPosition)),
                geometricNormal);
        if (dot(receiverPlaneNormal, geometricNormal) < 0.0) {
            receiverPlaneNormal = -receiverPlaneNormal;
        }
    }
    vec4 sampled = texture(texture0, fragTexCoord);
    vec3 ambientWeights = geometricNormal * geometricNormal;
    vec3 objectProbeLighting =
            objectAmbientCube[geometricNormal.x >= 0.0 ? 0 : 1] * ambientWeights.x
            + objectAmbientCube[geometricNormal.y >= 0.0 ? 2 : 3] * ambientWeights.y
            + objectAmbientCube[geometricNormal.z >= 0.0 ? 4 : 5] * ambientWeights.z;
    vec3 staticProbeLighting = useObjectAmbientCube != 0
            ? max(objectProbeLighting, vec3(0.0))
            : max(fragColor.rgb, vec3(0.0));
    vec3 tint = clamp(doorTint.rgb, 0.0, 1.0);
    vec3 surfaceRgb = sampled.rgb * tint;
    vec3 tangentNormalSample = hasNormalMap != 0
            ? texture(normalTexture, fragTexCoord).xyz
            : vec3(0.5, 0.5, 1.0);
    vec3 worldNormal = SurfaceNormal(
            geometricNormal, tangentNormalSample);
    vec3 viewDirection = SafeNormalize(
            cameraPosition - fragWorldPosition, geometricNormal);
    float materialAo = 1.0;
    float metallic = clamp(metallicFactor, 0.0, 1.0);
    float roughness = clamp(roughnessFactor, 0.045, 1.0);
    if (materialPropertiesKind == 1) {
        roughness = clamp(
                texture(materialPropertiesTexture, fragTexCoord).r,
                0.045, 1.0);
    } else if (materialPropertiesKind == 2) {
        vec3 orm = texture(materialPropertiesTexture, fragTexCoord).rgb;
        materialAo = clamp(orm.r, 0.0, 1.0);
        roughness = clamp(orm.g, 0.045, 1.0);
        metallic = clamp(orm.b, 0.0, 1.0);
    }
    vec3 f0 = mix(vec3(0.04), surfaceRgb, metallic);
    vec3 indirectDiffuse = surfaceRgb
            * (1.0 - metallic)
            * staticProbeLighting
            * indirectDiffuseScale
            * materialAo;
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
                coneAtten *= mix(1.0, visibility, clamp(shadowStrength[shadowSlot], 0.0, 1.0));
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

    vec3 environmentSpecular=SampleSectorEnvironment(fragWorldPosition,
            reflect(-viewDirection,worldNormal),roughness);
    vec2 brdf=EnvironmentBrdfApprox(roughness,max(dot(worldNormal,viewDirection),0.0));
    environmentSpecular *= (f0*brdf.x+brdf.y)*environmentSpecularScale;

    vec3 outputRgb = indirectDiffuse
            + dynamicDirectDiffuse
            + dynamicDirectSpecular
            + staticDirectSpecular
            + environmentSpecular;
    if (pbrDiagnosticMode == 11) outputRgb = indirectDiffuse + dynamicDirectDiffuse;
    else if (pbrDiagnosticMode == 1) outputRgb = surfaceRgb;
    else if (pbrDiagnosticMode == 2) outputRgb = dynamicDirectDiffuse;
    else if (pbrDiagnosticMode == 3) {
        outputRgb = dynamicDirectSpecular + staticDirectSpecular;
    }
    else if (pbrDiagnosticMode == 4) outputRgb = indirectDiffuse;
    else if (pbrDiagnosticMode == 5) outputRgb = environmentSpecular;
    else if (pbrDiagnosticMode == 6) outputRgb = vec3(0.0);
    else if (pbrDiagnosticMode == 7) outputRgb = vec3(materialAo);
    else if (pbrDiagnosticMode == 8) {
        outputRgb = vec3(metallic, roughness, 0.0);
    }
    else if (pbrDiagnosticMode == 9) {
        outputRgb = worldNormal * 0.5 + 0.5;
    }
    else if (pbrDiagnosticMode == 10) {
        outputRgb = hasNormalMap != 0
                ? tangentNormalSample
                : vec3(1.0, 0.0, 1.0);
    }
    if (pbrDiagnosticMode == 0) {
        outputRgb = ApplySectorFog(
                outputRgb,
                staticProbeLighting,
                fragWorldPosition);
    }
    finalColor = vec4(
            StoreFiniteHalfRadiance(outputRgb),
            clamp(sampled.a * doorTint.a, 0.0, 1.0));
}
