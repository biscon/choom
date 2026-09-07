#version 330
#include "reflection_sampling.glsl"
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
uniform sampler2D materialPropertiesTexture;
uniform sampler2D directionalLightmapTexture;
uniform samplerCube environmentTexture;
uniform float useLightmap;
uniform float useBakedAmbientOcclusion;
uniform int hasLightmap;
uniform int hasDirectionalLightmap;
uniform int hasNormalMap;
uniform float normalStrength;
uniform int materialPropertiesKind;
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

#include "surface_fog.glsl"

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
    vec3 correctedBakedLighting = ApplyDirectionalLightmap(
            bakedSample.rgb, geometricNormal, worldNormal);
    vec3 staticLighting = max(
            fragColor.rgb * aoFactor * materialAo + correctedBakedLighting,
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
    if (environmentSpecularScale > 0.0) {
        vec3 environment = SampleSectorEnvironment(fragWorldPosition, reflect(-viewDirection,worldNormal),roughness);
        vec2 brdf = EnvironmentBrdfApprox(roughness,max(dot(worldNormal,viewDirection),0.0));
        environmentSpecular = environment * (f0*brdf.x+brdf.y) * environmentSpecularScale;
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
    if (pbrDiagnosticMode == 11) surfaceOutput = (staticDiffuse + dynamicDirectDiffuse) * (1.0 - emissiveDecalAlpha) + emissiveRadiance * emissiveDecalAlpha;
    else if (pbrDiagnosticMode == 1) surfaceOutput = surfaceRgb;
    else if (pbrDiagnosticMode == 2) surfaceOutput = dynamicDirectDiffuse;
    else if (pbrDiagnosticMode == 3) {
        surfaceOutput = dynamicDirectSpecular + staticDirectSpecular;
    }
    else if (pbrDiagnosticMode == 4) surfaceOutput = staticDiffuse;
    else if (pbrDiagnosticMode == 5) surfaceOutput = environmentSpecular;
    else if (pbrDiagnosticMode == 6) surfaceOutput = emissiveRadiance;
    else if (pbrDiagnosticMode == 7) surfaceOutput = vec3(materialAo);
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
