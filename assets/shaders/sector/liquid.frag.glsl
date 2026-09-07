#version 330
#include "reflection_sampling.glsl"
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

uniform vec3 cameraPosition;
uniform float runtimeSeconds;
uniform vec3 shallowColor;
uniform vec3 deepColor;
// visibility depth, roughness, refraction strength, unused
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

const float OpaqueDepthEpsilon = 0.000001;

#include "safe_normalize_atmosphere.glsl"

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
        if (opaqueDepth + OpaqueDepthEpsilon < gl_FragCoord.z) discard;
    }

    vec3 normal = ProceduralNormal();
    vec3 viewDirection = SafeNormalize(cameraPosition - fragWorldPosition,
            vec3(0.0, 1.0, 0.0));
    if (dot(normal, viewDirection) < 0.0) normal = -normal;
    float ndotv = clamp(dot(normal, viewDirection), 0.0, 1.0);
    float fresnel = 0.02 + 0.98 * pow(1.0 - ndotv, 5.0);
    float roughness = clamp(liquidParams0.y, 0.02, 1.0);

    vec3 reflection = mix(deepColor, shallowColor, 0.35);
    if (rpMode != 0) reflection = SampleSectorEnvironment(fragWorldPosition,
            reflect(-viewDirection,normal),roughness)*environmentSpecularScale;
    reflection += DirectionalHighlight(normal, viewDirection, roughness);

    float opticalDepth = liquidParams0.x;
    vec3 transmitted = shallowColor;
    if (advancedTransmission != 0) {
        vec2 distortion = normal.xz * liquidParams0.z
                * mix(0.25, 1.0, 1.0 - ndotv);
        vec2 refractedUv = clamp(baseUv + distortion,
                vec2(0.001), vec2(0.999));
        float refractedDepth = texture(sceneDepth, refractedUv).r;
        if (refractedDepth + OpaqueDepthEpsilon < gl_FragCoord.z) {
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
    }

    vec3 rgb = mix(transmitted, reflection, fresnel);
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
