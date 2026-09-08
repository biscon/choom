#version 330
#include "reflection_sampling.glsl"
in vec2 fragTexCoord;
in vec2 fragLightmapTexCoord;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec3 fragWorldTangent;
in float fragTangentSign;
in vec4 fragColor;

uniform sampler2D baseColorTexture;
uniform sampler2D metallicTexture;
uniform sampler2D normalTexture;
uniform sampler2D roughnessTexture;
uniform sampler2D occlusionTexture;
uniform sampler2D emissiveTexture;
uniform sampler2D lightmapTexture;
uniform samplerCube environmentTexture;
uniform vec4 baseColorFactor;
uniform vec3 emissiveFactor;
uniform float emissiveStrength;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform float normalScale;
uniform float occlusionStrength;
uniform float modelOpacity;
uniform float interactionHighlightStrength;
uniform int hasBaseColorTexture;
uniform int hasMetallicTexture;
uniform int hasNormalTexture;
uniform int hasRoughnessTexture;
uniform int hasOcclusionTexture;
uniform int hasEmissiveTexture;
uniform int baseColorTextureHardwareSrgb;
uniform int emissiveTextureHardwareSrgb;
uniform int hasEnvironment;
uniform vec3 cameraPosition;
uniform float environmentExposure;
uniform float outputBrightnessMultiplier;
uniform vec3 containingSectorAmbient;
uniform int hasStaticLightmap;
uniform int useBakedAmbientOcclusion;
uniform int useObjectProbeLighting;
uniform vec3 objectAmbientCube[6];
uniform vec3 objectAmbientCubeUpper[6];
uniform float objectAmbientCubeLowerHeight;
uniform float objectAmbientCubeUpperHeight;
uniform int useVerticalObjectProbeLighting;
uniform int pbrDiagnosticMode;
uniform int specularAaEnabled;
uniform float indirectDiffuseScale;
uniform float environmentSpecularScale;
uniform int environmentBoxProjection;
uniform vec3 environmentCapturePosition;
uniform vec3 environmentInfluenceCenter;
uniform vec3 environmentHalfExtents;
uniform float environmentYaw;
uniform float environmentMaxLod;
uniform float environmentIntensity;
uniform int useStaticSpecularLighting;

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
    vec2(-0.326, -0.406), vec2(-0.840, -0.074),
    vec2(-0.696,  0.457), vec2(-0.203,  0.621),
    vec2( 0.962, -0.195), vec2( 0.473, -0.480),
    vec2( 0.519,  0.767), vec2( 0.185, -0.893),
    vec2( 0.507,  0.064), vec2( 0.896,  0.412),
    vec2(-0.322, -0.933), vec2(-0.792, -0.598)
);

#include "safe_normalize.glsl"


#include "flashlight_profile.glsl"

vec3 SrgbToLinear(vec3 value)
{
    bvec3 cutoff = lessThanEqual(value, vec3(0.04045));
    vec3 low = value / 12.92;
    vec3 high = pow((value + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, cutoff);
}

vec3 DecodeColorTexture(vec3 value, int hardwareSrgb)
{
    return hardwareSrgb != 0 ? value : SrgbToLinear(value);
}

#include "pbr.glsl"



vec3 EvaluateObjectAmbientCube(vec3 normal)
{
    vec3 weights = normal * normal;
    vec3 xLighting = objectAmbientCube[normal.x >= 0.0 ? 0 : 1];
    vec3 yLighting = objectAmbientCube[normal.y >= 0.0 ? 2 : 3];
    vec3 zLighting = objectAmbientCube[normal.z >= 0.0 ? 4 : 5];
    vec3 lowerLighting = xLighting * weights.x
            + yLighting * weights.y
            + zLighting * weights.z;
    if (useVerticalObjectProbeLighting == 0) return lowerLighting;

    vec3 upperXLighting = objectAmbientCubeUpper[normal.x >= 0.0 ? 0 : 1];
    vec3 upperYLighting = objectAmbientCubeUpper[normal.y >= 0.0 ? 2 : 3];
    vec3 upperZLighting = objectAmbientCubeUpper[normal.z >= 0.0 ? 4 : 5];
    vec3 upperLighting = upperXLighting * weights.x
            + upperYLighting * weights.y
            + upperZLighting * weights.z;
    float heightRange = objectAmbientCubeUpperHeight - objectAmbientCubeLowerHeight;
    float blend = heightRange > 0.0001
            ? clamp((fragWorldPosition.y - objectAmbientCubeLowerHeight) / heightRange, 0.0, 1.0)
            : 0.0;
    return mix(lowerLighting, upperLighting, blend);
}

vec3 EvaluateFogObjectProbeLighting()
{
    vec3 lowerLighting = (
            objectAmbientCube[0]
            + objectAmbientCube[1]
            + objectAmbientCube[2]
            + objectAmbientCube[4]
            + objectAmbientCube[5]) / 5.0;
    if (useVerticalObjectProbeLighting == 0) return lowerLighting;

    vec3 upperLighting = (
            objectAmbientCubeUpper[0]
            + objectAmbientCubeUpper[1]
            + objectAmbientCubeUpper[2]
            + objectAmbientCubeUpper[4]
            + objectAmbientCubeUpper[5]) / 5.0;
    float heightRange = objectAmbientCubeUpperHeight - objectAmbientCubeLowerHeight;
    float blend = heightRange > 0.0001
            ? clamp((fragWorldPosition.y - objectAmbientCubeLowerHeight) / heightRange, 0.0, 1.0)
            : 0.0;
    return mix(lowerLighting, upperLighting, blend);
}

vec3 ShapeModelEmissive(
        vec3 emissive,
        vec3 geometricNormal,
        vec3 viewDirection)
{
    float facing = clamp(abs(dot(geometricNormal, viewDirection)), 0.0, 1.0);
    float edgeFactor = mix(0.70, 1.0, smoothstep(0.0, 0.60, facing));
    emissive *= edgeFactor;
    float peak = max(emissive.r, max(emissive.g, emissive.b));
    float whitening = 0.70 * smoothstep(1.0, 4.0, peak);
    return mix(emissive, vec3(peak), whitening);
}


#include "dynamic_shadow.glsl"

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
    vec3 tangent = SafeNormalize(
            fragWorldTangent - geometricNormal * dot(fragWorldTangent, geometricNormal),
            SafeNormalize(cross(abs(geometricNormal.y) < 0.999
                    ? vec3(0.0, 1.0, 0.0)
                    : vec3(1.0, 0.0, 0.0), geometricNormal), vec3(1.0, 0.0, 0.0)));
    vec3 bitangent = SafeNormalize(cross(geometricNormal, tangent), vec3(0.0, 0.0, 1.0))
            * (fragTangentSign < 0.0 ? -1.0 : 1.0);
    vec3 tangentNormalSample = vec3(0.5, 0.5, 1.0);
    vec3 worldNormal = geometricNormal;
    if (hasNormalTexture != 0) {
        tangentNormalSample = texture(normalTexture, fragTexCoord).xyz;
        vec3 mappedNormal = tangentNormalSample * 2.0 - 1.0;
        mappedNormal.xy *= normalScale;
        worldNormal = SafeNormalize(
                mat3(tangent, bitangent, geometricNormal) * mappedNormal,
                geometricNormal);
    }

    vec4 sampledBase = hasBaseColorTexture != 0
            ? texture(baseColorTexture, fragTexCoord)
            : vec4(1.0);
    vec3 albedo = DecodeColorTexture(
            sampledBase.rgb,
            baseColorTextureHardwareSrgb)
            * baseColorFactor.rgb
            * fragColor.rgb;
    float metallic = clamp(metallicFactor
            * (hasMetallicTexture != 0 ? texture(metallicTexture, fragTexCoord).r : 1.0),
            0.0, 1.0);
    float roughness = clamp(roughnessFactor
            * (hasRoughnessTexture != 0 ? texture(roughnessTexture, fragTexCoord).r : 1.0),
            0.045, 1.0);
    float materialAo = hasOcclusionTexture != 0
            ? mix(1.0, texture(occlusionTexture, fragTexCoord).r, occlusionStrength)
            : 1.0;
    vec3 viewDirection = SafeNormalize(cameraPosition - fragWorldPosition, geometricNormal);
    float specularRoughness = FilterSpecularRoughness(
            roughness, worldNormal, specularAaEnabled != 0);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 directDiffuse = vec3(0.0);
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
            vec3 lightDirection = distanceToLight > 0.0001
                    ? toLight / distanceToLight
                    : worldNormal;
            float ndotl = max(dot(worldNormal, lightDirection), 0.0);
            float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
            atten *= atten;
            if (ndotl <= 0.0 || atten <= 0.0
                    || dynamicLightIntensities[i] <= 0.0) continue;
            float coneAtten = emitterAtten;
            if (dynamicLightTypes[i] == 1) {
                vec3 spotDirection = SafeNormalize(
                        dynamicLightDirections[i],
                        vec3(0.0, -1.0, 0.0));
                vec3 fragmentDirectionFromLight = distanceToLight > 0.0001
                        ? -lightDirection
                        : spotDirection;
                float coneDot = dot(spotDirection, fragmentDirectionFromLight);
                if (dynamicLightProfiles[i] == 1) {
                    coneAtten = FlashlightProfileFactor(
                            i, fragmentDirectionFromLight);
                } else {
                    coneAtten = abs(dynamicLightInnerConeCos[i]
                                    - dynamicLightOuterConeCos[i]) > 0.0001
                            ? smoothstep(
                                    dynamicLightOuterConeCos[i],
                                    dynamicLightInnerConeCos[i],
                                    coneDot)
                            : step(dynamicLightInnerConeCos[i], coneDot);
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
            {
                vec3 halfway = SafeNormalize(viewDirection + lightDirection, worldNormal);
                float distribution = DistributionGgx(worldNormal, halfway, specularRoughness);
                float geometry = GeometrySmith(worldNormal, viewDirection, lightDirection, specularRoughness);
                vec3 fresnel = FresnelSchlick(max(dot(halfway, viewDirection), 0.0), f0);
                vec3 specular = distribution * geometry * fresnel
                        / max(4.0 * max(dot(worldNormal, viewDirection), 0.0) * ndotl, 0.001);
                vec3 diffuseWeight = (vec3(1.0) - fresnel) * (1.0 - metallic);
                vec3 radiance = dynamicLightColors[i]
                        * dynamicLightIntensities[i]
                        * atten
                        * coneAtten;
                directDiffuse += diffuseWeight * albedo * radiance * ndotl;
                dynamicDirectSpecular += specular * radiance * ndotl;
            }
        }
    }

    vec3 staticDirectSpecular = vec3(0.0);
    if (useStaticSpecularLighting != 0) {
        for (int i = 0;
                i < staticSpecularLightCount
                        && i < MAX_STATIC_SPECULAR_LIGHTS;
                ++i) {
            float radius = staticSpecularLightRadii[i];
            vec3 toLight = staticSpecularLightPositions[i] - fragWorldPosition;
            float distanceSq = dot(toLight, toLight);
            if (radius > 0.0 && distanceSq < radius * radius) {
                float distanceToLight = sqrt(max(distanceSq, 0.0));
                vec3 lightDirection = distanceToLight > 0.0001
                        ? toLight / distanceToLight
                        : worldNormal;
                float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
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
                            spotDirection,
                            fragmentDirectionFromLight);
                    coneAtten = abs(
                            staticSpecularLightInnerConeCos[i]
                                    - staticSpecularLightOuterConeCos[i]) > 0.0001
                            ? smoothstep(
                                    staticSpecularLightOuterConeCos[i],
                                    staticSpecularLightInnerConeCos[i],
                                    coneDot)
                            : step(
                                    staticSpecularLightInnerConeCos[i],
                                    coneDot);
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
                float ndotl = max(dot(worldNormal, lightDirection), 0.0);
                if (ndotl > 0.0 && coneAtten > 0.0) {
                    vec3 halfway = SafeNormalize(
                            viewDirection + lightDirection,
                            worldNormal);
                    float distribution = DistributionGgx(
                            worldNormal, halfway, specularRoughness);
                    float geometry = GeometrySmith(
                            worldNormal,
                            viewDirection,
                            lightDirection,
                            specularRoughness);
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
        }
    }

    vec4 bakedStaticSample = hasStaticLightmap != 0
            ? texture(lightmapTexture, fragLightmapTexCoord)
            : vec4(0.0, 0.0, 0.0, 1.0);
    vec3 staticLighting = containingSectorAmbient;
    vec3 staticAtmosphericLighting = containingSectorAmbient;
    if (useObjectProbeLighting != 0) {
        staticLighting = EvaluateObjectAmbientCube(worldNormal);
        staticAtmosphericLighting = EvaluateFogObjectProbeLighting();
    } else if (hasStaticLightmap != 0) {
        float ao = useBakedAmbientOcclusion != 0 ? bakedStaticSample.a : 1.0;
        staticLighting = containingSectorAmbient * ao + bakedStaticSample.rgb;
        staticAtmosphericLighting = containingSectorAmbient + bakedStaticSample.rgb;
    }
    // Probe, fallback ambient, and lightmap RGB are incoming diffuse
    // illumination. They never enter final radiance without the material's
    // diffuse base-color, non-metal, AO, and renderer scale.
    vec3 indirectDiffuse = albedo
            * (1.0 - metallic)
            * staticLighting
            * materialAo
            * indirectDiffuseScale;
    vec3 environmentSpecular=SampleSectorEnvironment(fragWorldPosition,
            reflect(-viewDirection,worldNormal),specularRoughness);
    vec2 brdf=EnvironmentBrdfApprox(specularRoughness,max(dot(worldNormal,viewDirection),0.0));
    environmentSpecular *= (f0*brdf.x+brdf.y)*environmentSpecularScale*materialAo;
    vec3 emissive = emissiveFactor;
    if (hasEmissiveTexture != 0) {
        emissive *= DecodeColorTexture(
                texture(emissiveTexture, fragTexCoord).rgb,
                emissiveTextureHardwareSrgb);
    }
    emissive *= max(emissiveStrength, 0.0);
    emissive = ShapeModelEmissive(
            emissive,
            geometricNormal,
            viewDirection);
    vec3 linearColor = indirectDiffuse
            + directDiffuse
            + dynamicDirectSpecular
            + staticDirectSpecular
            + environmentSpecular
            + emissive;
    if (pbrDiagnosticMode == 11) linearColor = indirectDiffuse + directDiffuse + emissive;
    else if (pbrDiagnosticMode == 1) linearColor = albedo;
    else if (pbrDiagnosticMode == 2) linearColor = directDiffuse;
    else if (pbrDiagnosticMode == 3) {
        linearColor = dynamicDirectSpecular + staticDirectSpecular;
    }
    else if (pbrDiagnosticMode == 4) linearColor = indirectDiffuse;
    else if (pbrDiagnosticMode == 5) linearColor = environmentSpecular;
    else if (pbrDiagnosticMode == 6) linearColor = emissive;
    else if (pbrDiagnosticMode == 7) linearColor = vec3(materialAo);
    else if (pbrDiagnosticMode == 8) linearColor = vec3(metallic, roughness, 0.0);
    else if (pbrDiagnosticMode == 9) linearColor = worldNormal * 0.5 + 0.5;
    else if (pbrDiagnosticMode == 10) {
        linearColor = hasNormalTexture != 0
                ? tangentNormalSample
                : vec3(1.0, 0.0, 1.0);
    }

    // Artistic/display ceilings are forbidden. The final write below applies
    // only the unavoidable finite RGBA16F storage boundary.
    linearColor = max(linearColor, vec3(0.0));
    if (pbrDiagnosticMode == 0) {
        // Treat the prop's own albedo as a small emissive-like lift. This
        // preserves material hue and saturation instead of washing the model
        // toward a neutral selection color.
        linearColor += albedo * clamp(
                interactionHighlightStrength,
                0.0,
                0.14);
        linearColor *= outputBrightnessMultiplier;
        linearColor = ApplySectorFog(
                linearColor,
                staticAtmosphericLighting,
                fragWorldPosition);
    }
    linearColor.r = isnan(linearColor.r) ? 0.0 : (isinf(linearColor.r) ? (linearColor.r > 0.0 ? 65504.0 : 0.0) : min(max(linearColor.r, 0.0), 65504.0));
    linearColor.g = isnan(linearColor.g) ? 0.0 : (isinf(linearColor.g) ? (linearColor.g > 0.0 ? 65504.0 : 0.0) : min(max(linearColor.g, 0.0), 65504.0));
    linearColor.b = isnan(linearColor.b) ? 0.0 : (isinf(linearColor.b) ? (linearColor.b > 0.0 ? 65504.0 : 0.0) : min(max(linearColor.b, 0.0), 65504.0));
    finalColor = vec4(
            linearColor,
            clamp(modelOpacity, 0.0, 1.0));
}
