#version 330
in vec2 fragTexCoord;
in vec3 fragWorldPosition;
in vec3 fragStaticLighting;
in vec4 fragParticleColor;
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
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform float nearPlane;
uniform float farPlane;
uniform vec3 cameraPosition;
uniform int fogEnabled;
uniform float fogStartDistance;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeight;
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
uniform vec3 dynamicLightSpotShadowRight[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform int shadowAtlasTilesPerRow;
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

const vec2 kShadowDisk[4] = vec2[4](
    vec2(-0.707, -0.707), vec2(0.707, -0.707),
    vec2(-0.707, 0.707), vec2(0.707, 0.707));

#include "safe_normalize_lower.glsl"
float linearDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) /
            max(farPlane + nearPlane - z * (farPlane - nearPlane), 0.00001);
}
float shadowDepth(int slot, vec2 uv) {
    int tiles=max(shadowAtlasTilesPerRow,1); vec2 tile=vec2(slot%tiles,slot/tiles);
    return texture(shadowMap0,(tile+clamp(uv,vec2(0.001),vec2(0.999)))/float(tiles)).r;
}
int pointShadowFace(vec3 ray) {
    vec3 magnitude=abs(ray);
    if(magnitude.x>=magnitude.y&&magnitude.x>=magnitude.z)return ray.x>=0.0?0:1;
    if(magnitude.y>=magnitude.z)return ray.y>=0.0?2:3;
    return ray.z>=0.0?4:5;
}
vec2 pointShadowUv(int face,vec3 ray) {
    vec3 magnitude=max(abs(ray),vec3(0.00001)); vec2 projected;
    if(face==0)projected=vec2(-ray.z,-ray.y)/magnitude.x;
    else if(face==1)projected=vec2(ray.z,-ray.y)/magnitude.x;
    else if(face==2)projected=vec2(ray.x,ray.z)/magnitude.y;
    else if(face==3)projected=vec2(ray.x,-ray.z)/magnitude.y;
    else if(face==4)projected=vec2(ray.x,-ray.y)/magnitude.z;
    else projected=vec2(-ray.x,-ray.y)/magnitude.z;
    return projected*0.5+0.5;
}
vec3 pointShadowRay(int face,vec2 uv) {
    vec2 projected=uv*2.0-1.0;
    if(face==0)return vec3(1.0,-projected.y,-projected.x);
    if(face==1)return vec3(-1.0,-projected.y,projected.x);
    if(face==2)return vec3(projected.x,1.0,projected.y);
    if(face==3)return vec3(projected.x,-1.0,-projected.y);
    if(face==4)return vec3(projected.x,-projected.y,1.0);
    return vec3(-projected.x,-projected.y,-1.0);
}
float pointShadowDepth(int baseSlot,int sourceFace,vec2 sourceUv,bool frontHemisphereOnly) {
    vec3 ray=pointShadowRay(sourceFace,sourceUv);
    int face=pointShadowFace(ray);
    if(frontHemisphereOnly&&face==5)return 1.0;
    return shadowDepth(baseSlot+face,pointShadowUv(face,ray));
}
float shadowVisibility(int lightIndex, int slot, vec3 position) {
    if (slot < 0 || slot >= MAX_DYNAMIC_SHADOW_CASTERS) return 1.0;
    vec3 fromLight=position-dynamicLightPositions[lightIndex];
    vec3 coordinate;
    int cubeFace=-1;
    bool rectProjection=dynamicLightTypes[lightIndex]==2;
    if(dynamicLightTypes[lightIndex]==0||rectProjection) {
        vec3 cubeFromLight=fromLight;
        float lightRadius=max(dynamicLightRadii[lightIndex],0.00001);
        if(rectProjection) {
            vec3 forward=safeNormalize(dynamicLightDirections[lightIndex],vec3(0,-1,0));
            vec3 right=safeNormalize(dynamicLightSpotShadowRight[lightIndex],vec3(1,0,0));
            vec3 emitterUp=safeNormalize(cross(right,forward),vec3(0,0,1));
            vec3 cubeUp=-emitterUp;
            cubeFromLight=vec3(dot(fromLight,right),dot(fromLight,cubeUp),dot(fromLight,forward));
            if(cubeFromLight.z<=0.0)return 1.0;
            lightRadius+=length(vec2(max(dynamicLightInnerConeCos[lightIndex],0.0),
                    max(dynamicLightOuterConeCos[lightIndex],0.0)));
        }
        vec3 magnitude=abs(cubeFromLight); float forwardDepth=max(magnitude.x,max(magnitude.y,magnitude.z));
        if(forwardDepth<=0.05||forwardDepth>lightRadius)return 1.0;
        cubeFace=pointShadowFace(cubeFromLight); if(rectProjection&&cubeFace==5)return 1.0;
        float farCoefficient=lightRadius/max(lightRadius-0.05,0.00001);
        coordinate=vec3(pointShadowUv(cubeFace,cubeFromLight),farCoefficient*(1.0-0.05/forwardDepth)); }
    else { vec3 forward=safeNormalize(dynamicLightDirections[lightIndex],vec3(0,-1,0));
        vec3 upReference=abs(forward.y)>0.98?vec3(0,0,1):vec3(0,1,0);
        vec3 right=safeNormalize(cross(forward,upReference),vec3(1,0,0));
        vec3 up=cross(right,forward); float z=dot(fromLight,forward); if(z<=0.05)return 1.0;
        float tangent=tan(min(acos(clamp(dynamicLightOuterConeCos[lightIndex],-0.999,0.999)),1.553343)); float farPlane=dynamicLightRadii[lightIndex];
        float ndc=(farPlane+0.05)/(farPlane-0.05)-(2.0*farPlane*0.05)/((farPlane-0.05)*z);
        coordinate=vec3(vec2(dot(fromLight,right),dot(fromLight,up))/max(2.0*z*tangent,0.00001)+0.5,ndc*0.5+0.5); }
    if (any(lessThan(coordinate, vec3(0.0))) || any(greaterThan(coordinate, vec3(1.0)))) return 1.0;
    float compareDepth = coordinate.z - min(max(shadowBias[slot], 0.0), 0.02);
    float softness = clamp(shadowSoftness[slot], 0.0, 8.0);
    if (softness <= 0.0) {
        float blockerDepth=cubeFace>=0?pointShadowDepth(slot,cubeFace,coordinate.xy,rectProjection):shadowDepth(slot,coordinate.xy);
        return compareDepth <= blockerDepth ? 1.0 : 0.0;
    }
    vec2 texel = vec2(float(max(shadowAtlasTilesPerRow,1))) / vec2(textureSize(shadowMap0, 0));
    float visible = 0.0;
    for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
        vec2 uv = coordinate.xy + kShadowDisk[sampleIndex] * max(0.25, softness) * texel;
        float blockerDepth=cubeFace>=0?pointShadowDepth(slot,cubeFace,uv,rectProjection):shadowDepth(slot,uv);
        visible += compareDepth <= blockerDepth ? 1.0 : 0.0;
    }
    return visible * 0.25;
}
vec3 dynamicLighting(vec3 position) {
    vec3 result = vec3(0.0);
    for (int index = 0; index < MAX_DYNAMIC_LIGHTS; ++index) {
        if (index >= dynamicLightCount) break;
        float radius = dynamicLightRadii[index];
        vec3 toLight = dynamicLightPositions[index] - position;
        float emitter = 1.0;
        if (dynamicLightTypes[index] == 2) {
            vec3 normal = safeNormalize(dynamicLightDirections[index], vec3(0,-1,0));
            vec3 right = safeNormalize(dynamicLightSpotShadowRight[index], vec3(1,0,0));
            vec3 up = safeNormalize(cross(right,normal), vec3(0,0,1));
            vec3 relative = position-dynamicLightPositions[index];
            vec3 nearest=dynamicLightPositions[index]
                    +right*clamp(dot(relative,right),-dynamicLightInnerConeCos[index],dynamicLightInnerConeCos[index])
                    +up*clamp(dot(relative,up),-dynamicLightOuterConeCos[index],dynamicLightOuterConeCos[index]);
            toLight=nearest-position;
            emitter=max(dot(normal,safeNormalize(position-nearest,normal)),0.0);
        }
        float distanceSquared = dot(toLight, toLight);
        if (radius <= 0.0 || distanceSquared >= radius * radius) continue;
        float distanceToLight = sqrt(max(distanceSquared, 0.0));
        vec3 lightDirection = distanceToLight > 0.0001 ? toLight / distanceToLight : vec3(0.0, 1.0, 0.0);
        float attenuation = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
        attenuation *= attenuation;
        float cone = emitter;
        if (dynamicLightTypes[index] == 1) {
            vec3 spotDirection = safeNormalize(dynamicLightDirections[index], vec3(0.0, -1.0, 0.0));
            float coneDot = dot(spotDirection, distanceToLight > 0.0001 ? -lightDirection : spotDirection);
            float inner = dynamicLightInnerConeCos[index];
            float outer = dynamicLightOuterConeCos[index];
            cone = abs(inner - outer) > 0.0001 ? smoothstep(outer, inner, coneDot) : step(inner, coneDot);
        }
        int shadowSlot = dynamicLightShadowSlots[index];
        if (shadowSlot >= 0 && cone > 0.0) {
            cone *= mix(1.0, shadowVisibility(index, shadowSlot, position),
                    clamp(shadowStrength[shadowSlot], 0.0, 1.0));
        }
        result += dynamicLightColors[index] * dynamicLightIntensities[index] * attenuation * cone;
    }
    return result;
}
float distanceFogTransmittance(vec3 position) {
    if (fogEnabled == 0 || fogDensity <= 0.0 || fogMaxOpacity <= 0.0) return 1.0;
    float distanceToCamera = max(length(position - cameraPosition) - fogStartDistance, 0.0);
    float midpointHeight = (cameraPosition.y + position.y) * 0.5;
    float aboveReference = max(midpointHeight - fogReferenceHeight, 0.0);
    float amount = min(1.0 - exp(-fogDensity * distanceToCamera *
            exp(-aboveReference * fogHeightFalloff)), fogMaxOpacity);
    return 1.0 - amount;
}
void main() {
    vec2 centered = fragTexCoord * 2.0 - 1.0;
    float radiusSquared = dot(centered, centered);
    if (radiusSquared >= 1.0) discard;
    float softMask = (1.0 - smoothstep(0.05, 1.0, radiusSquared));
    softMask *= softMask;
    vec2 screenUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    float opaqueDepth = texture(sceneDepth, screenUv).r;
    float opaqueDistance = linearDepth(opaqueDepth);
    float particleDistance = linearDepth(gl_FragCoord.z);
    float intersectionFade = clamp((opaqueDistance - particleDistance) / 0.08, 0.0, 1.0);
    if (intersectionFade <= 0.0) discard;
    vec3 illumination = max(fragStaticLighting + dynamicLighting(fragWorldPosition), vec3(0.0));
    float opacity = fragParticleColor.a * softMask * intersectionFade;
    vec3 srgb = fragParticleColor.rgb;
    vec3 low = srgb / 12.92;
    vec3 high = pow((srgb + 0.055) / 1.055, vec3(2.4));
    vec3 authoredLinearTint = mix(high, low, lessThanEqual(srgb, vec3(0.04045)));
    vec3 scattered = authoredLinearTint * illumination *
            distanceFogTransmittance(fragWorldPosition);
    finalColor = vec4(StoreFiniteHalfRadiance(scattered * opacity), 0.0);
}
