#include "sector_demo/renderer/SectorVolumetricAtmosphereRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereContracts.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace game {
namespace {

const char* FullscreenVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragUv;
uniform mat4 mvp;
void main() { fragUv=vertexTexCoord; gl_Position=mvp*vec4(vertexPosition,1.0); }
)";

const char* UnifiedAtmosphereFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;

#define MAX_STEPS 64
#define MAX_LOCAL_VOLUMES 16
#define MAX_DYNAMIC_LIGHTS 8
#define MAX_DYNAMIC_SHADOW_CASTERS 2

uniform sampler2D sceneDepth;
uniform int marchSteps;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform float nearPlane;
uniform float farPlane;
uniform float maximumDistance;
uniform float anisotropy;
uniform float runtimeSeconds;

uniform int fogEnabled;
uniform vec3 fogColor;
uniform float fogStartDistance;
uniform float fogDensity;
uniform float fogMaximumOpacity;
uniform float fogReferenceHeight;
uniform float fogHeightFalloff;

uniform int localVolumeCount;
uniform vec3 localCenters[MAX_LOCAL_VOLUMES];
uniform vec3 localRadii[MAX_LOCAL_VOLUMES];
uniform vec3 localColors[MAX_LOCAL_VOLUMES];
uniform vec4 localParamsA[MAX_LOCAL_VOLUMES];
uniform vec4 localParamsB[MAX_LOCAL_VOLUMES];

uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightTypes[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightDirections[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];
uniform mat4 shadowLightMatrices[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

const vec2 kShadowDisk[4]=vec2[4](
    vec2(-0.707,-0.707),vec2(0.707,-0.707),
    vec2(-0.707,0.707),vec2(0.707,0.707));
const float kPi=3.14159265358979323846;

float finiteHalf(float v) {
    if(isnan(v)) return 0.0;
    if(isinf(v)) return v>0.0?65504.0:0.0;
    return clamp(v,0.0,65504.0);
}
vec3 finiteRadiance(vec3 v) { return vec3(finiteHalf(v.r),finiteHalf(v.g),finiteHalf(v.b)); }
vec3 safeNormalize(vec3 v,vec3 fallback) {
    float l2=dot(v,v); return l2>0.00000001?v*inversesqrt(l2):fallback;
}
float hash31(vec3 p) {
    p=fract(p*0.1031); p+=dot(p,p.yzx+33.33); return fract((p.x+p.y)*p.z);
}
float valueNoise(vec3 p) {
    vec3 c=floor(p); vec3 f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(mix(hash31(c),hash31(c+vec3(1,0,0)),f.x),
                   mix(hash31(c+vec3(0,1,0)),hash31(c+vec3(1,1,0)),f.x),f.y),
               mix(mix(hash31(c+vec3(0,0,1)),hash31(c+vec3(1,0,1)),f.x),
                   mix(hash31(c+vec3(0,1,1)),hash31(c+vec3(1,1,1)),f.x),f.y),f.z);
}
float capDepth(float maximumOpacity) {
    return -log(max(1.0-clamp(maximumOpacity,0.0,0.9999),0.0001));
}
float phaseHg(float cosineTheta) {
    float g=clamp(anisotropy,-0.90,0.90);
    float d=max(1.0+g*g-2.0*g*clamp(cosineTheta,-1.0,1.0),0.000001);
    return (1.0-g*g)/(4.0*kPi*d*sqrt(d));
}
float shadowDepthAt(int slot,vec2 uv) {
    return slot==0?texture(shadowMap0,uv).r:texture(shadowMap1,uv).r;
}
float shadowVisibility(int slot,vec3 p) {
    if(slot<0||slot>=MAX_DYNAMIC_SHADOW_CASTERS) return 1.0;
    vec4 clip=shadowLightMatrices[slot]*vec4(p,1.0);
    if(clip.w<=0.0) return 1.0;
    vec3 sc=clip.xyz/clip.w*0.5+0.5;
    if(any(lessThan(sc,vec3(0.0)))||any(greaterThan(sc,vec3(1.0)))) return 1.0;
    float compare=sc.z-clamp(shadowBias[slot],0.0,0.02);
    float softness=clamp(shadowSoftness[slot],0.0,8.0);
    if(softness<=0.0) return compare<=shadowDepthAt(slot,sc.xy)?1.0:0.0;
    vec2 texel=1.0/vec2(textureSize(shadowMap0,0)); float visible=0.0;
    for(int i=0;i<4;++i) {
        vec2 uv=clamp(sc.xy+kShadowDisk[i]*max(0.25,softness)*texel,vec2(0),vec2(1));
        visible+=compare<=shadowDepthAt(slot,uv)?1.0:0.0;
    }
    return visible*0.25;
}
vec3 dynamicLighting(vec3 p,vec3 rayDirection) {
    vec3 result=vec3(0.0);
    for(int i=0;i<MAX_DYNAMIC_LIGHTS;++i) {
        if(i>=dynamicLightCount) break;
        float radius=dynamicLightRadii[i]; vec3 toLight=dynamicLightPositions[i]-p;
        float distance2=dot(toLight,toLight);
        if(radius<=0.0||distance2>=radius*radius) continue;
        float distanceToLight=sqrt(max(distance2,0.0));
        vec3 lightDirection=distanceToLight>0.0001?toLight/distanceToLight:vec3(0,1,0);
        float attenuation=clamp(1.0-distanceToLight/radius,0.0,1.0); attenuation*=attenuation;
        float cone=1.0;
        if(dynamicLightTypes[i]==1) {
            vec3 spotDirection=safeNormalize(dynamicLightDirections[i],vec3(0,-1,0));
            float coneDot=dot(spotDirection,distanceToLight>0.0001?-lightDirection:spotDirection);
            float inner=dynamicLightInnerConeCos[i],outer=dynamicLightOuterConeCos[i];
            cone=abs(inner-outer)>0.0001?smoothstep(outer,inner,coneDot):step(inner,coneDot);
            int slot=dynamicLightShadowSlots[i];
            if(slot>=0&&cone>0.0) cone*=mix(1.0,shadowVisibility(slot,p),clamp(shadowStrength[slot],0.0,1.0));
        }
        float phase=phaseHg(dot(lightDirection,-rayDirection));
        result+=dynamicLightColors[i]*dynamicLightIntensities[i]*attenuation*cone*phase;
    }
    return result;
}
void addMedium(float rawDepth,float maximumOpacity,vec3 tint,
        inout float accumulatedDepth,inout float totalDepth,
        inout vec3 weightedTint) {
    float accepted=min(max(rawDepth,0.0),max(capDepth(maximumOpacity)-accumulatedDepth,0.0));
    if(accepted<=0.0) return;
    accumulatedDepth+=accepted; totalDepth+=accepted;
    weightedTint+=tint*accepted;
}
void main() {
    float depth=texture(sceneDepth,fragUv).r; vec2 ndc=fragUv*2.0-1.0;
    vec3 rayDirection=normalize(cameraForward+cameraRight*ndc.x*tanHalfFov*aspectRatio+cameraUp*ndc.y*tanHalfFov);
    float sceneDistance=maximumDistance;
    if(!isnan(depth)&&!isinf(depth)&&depth<0.999999) {
        float z=clamp(depth,0.0,1.0)*2.0-1.0;
        float forwardDistance=(2.0*nearPlane*farPlane)/max(farPlane+nearPlane-z*(farPlane-nearPlane),0.00001);
        sceneDistance=min(maximumDistance,forwardDistance/max(dot(rayDirection,cameraForward),0.0001));
    }
    sceneDistance=max(sceneDistance,0.0);
    int steps=max(marchSteps,1); float stepLength=sceneDistance/float(steps);
    float globalDepth=0.0; float localDepths[MAX_LOCAL_VOLUMES];
    for(int i=0;i<MAX_LOCAL_VOLUMES;++i) localDepths[i]=0.0;
    float transmittance=1.0; vec3 radiance=vec3(0.0);
    for(int stepIndex=0;stepIndex<MAX_STEPS;++stepIndex) {
        if(stepIndex>=steps||transmittance<=0.0001) break;
        float t=(float(stepIndex)+0.5)*stepLength; vec3 p=cameraPosition+rayDirection*t;
        float totalDepth=0.0; vec3 weightedTint=vec3(0.0);
        if(fogEnabled!=0&&t>fogStartDistance&&fogDensity>0.0&&fogMaximumOpacity>0.0) {
            float above=max(p.y-fogReferenceHeight,0.0);
            float raw=fogDensity*exp(-above*fogHeightFalloff)*stepLength;
            addMedium(raw,fogMaximumOpacity,fogColor,globalDepth,totalDepth,weightedTint);
        }
        for(int i=0;i<MAX_LOCAL_VOLUMES;++i) {
            if(i>=localVolumeCount) break;
            vec3 local=(p-localCenters[i])/max(localRadii[i],vec3(0.0001)); float radius=length(local);
            if(radius>1.0) continue;
            vec4 a=localParamsA[i],b=localParamsB[i]; float boundary=1.0-smoothstep(1.0-max(a.z,0.0001),1.0,radius);
            vec2 flow=vec2(cos(b.y),sin(b.y))*b.z*runtimeSeconds; vec3 np=p/max(a.w,0.05); np.xz+=flow;
            float noise=mix(1.0,mix(0.35,1.35,valueNoise(np)),b.x);
            addMedium(a.x*boundary*noise*stepLength,a.y,localColors[i],
                    localDepths[i],totalDepth,weightedTint);
        }
        if(totalDepth<=0.0) continue;
        vec3 direct=dynamicLighting(p,rayDirection);
        vec3 source=weightedTint*direct/max(totalDepth,0.000001);
        float stepTransmittance=exp(-totalDepth); float stepOpacity=1.0-stepTransmittance;
        radiance+=transmittance*stepOpacity*max(source,vec3(0.0)); transmittance*=stepTransmittance;
    }
    finalColor=vec4(finiteRadiance(radiance),clamp(1.0-transmittance,0.0,1.0));
}
)";

const char* CompositeFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D atmosphereTexture;
uniform vec2 atmosphereTexelSize;
float finiteHalf(float v) {
    if(isnan(v)) return 0.0;
    if(isinf(v)) return v>0.0?65504.0:0.0;
    return clamp(v,0.0,65504.0);
}
vec3 finiteRadiance(vec3 v) { return vec3(finiteHalf(v.r),finiteHalf(v.g),finiteHalf(v.b)); }
void main() {
    float centerDepth=texture(sceneDepth,fragUv).r; vec4 atmosphere=vec4(0); float totalWeight=0.0;
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
        if(x!=0&&y!=0) continue;
        vec2 uv=clamp(fragUv+vec2(x,y)*atmosphereTexelSize,vec2(0),vec2(1));
        float sampleDepth=texture(sceneDepth,uv).r;
        float difference=(isnan(centerDepth)||isinf(centerDepth)||isnan(sampleDepth)||isinf(sampleDepth))?0.0:abs(sampleDepth-centerDepth);
        float weight=exp(-difference*600.0); atmosphere+=texture(atmosphereTexture,uv)*weight; totalWeight+=weight;
    }
    atmosphere/=max(totalWeight,0.0001); vec4 scene=texture(sceneColor,fragUv);
    float opacity=(isnan(atmosphere.a)||isinf(atmosphere.a))?0.0:clamp(atmosphere.a,0.0,1.0);
    vec3 composed=finiteRadiance(scene.rgb)*(1.0-opacity)+finiteRadiance(atmosphere.rgb);
    float alpha=(isnan(scene.a)||isinf(scene.a))?1.0:clamp(scene.a,0.0,1.0);
    finalColor=vec4(finiteRadiance(composed),alpha);
}
)";

Rectangle SourceRect(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f,
            static_cast<float>(texture.width),
            -static_cast<float>(texture.height)};
}

Rectangle DestinationRect(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f,
            static_cast<float>(texture.width),
            static_cast<float>(texture.height)};
}

int ArrayLocation(Shader shader, const char* name)
{
    const int direct = GetShaderLocation(shader, name);
    if (direct >= 0) return direct;
    return GetShaderLocation(shader, (std::string(name) + "[0]").c_str());
}

int ArrayElementLocation(Shader shader, const char* name, std::size_t index)
{
    return GetShaderLocation(shader,
            (std::string(name) + "[" + std::to_string(index) + "]").c_str());
}

} // namespace

bool SectorVolumetricAtmosphereRenderer::Initialize()
{
    if (shader.id != 0 && compositeShader.id != 0) return true;
    if (shaderFailed) return false;
    shader = LoadShaderFromMemory(FullscreenVs, UnifiedAtmosphereFs);
    compositeShader = LoadShaderFromMemory(FullscreenVs, CompositeFs);
    if (shader.id == 0 || compositeShader.id == 0) {
        if (shader.id != 0) UnloadShader(shader);
        if (compositeShader.id != 0) UnloadShader(compositeShader);
        shader = {};
        compositeShader = {};
        shaderFailed = true;
        resourceDiagnostic = "disabled: unified shader unavailable";
        return false;
    }
#define LOC(field, name) locations.field = GetShaderLocation(shader, name)
    LOC(sceneDepth, "sceneDepth"); LOC(marchSteps, "marchSteps");
    LOC(cameraPosition, "cameraPosition"); LOC(cameraForward, "cameraForward");
    LOC(cameraRight, "cameraRight"); LOC(cameraUp, "cameraUp");
    LOC(tanHalfFov, "tanHalfFov"); LOC(aspectRatio, "aspectRatio");
    LOC(nearPlane, "nearPlane"); LOC(farPlane, "farPlane");
    LOC(maximumDistance, "maximumDistance"); LOC(anisotropy, "anisotropy");
    LOC(runtimeSeconds, "runtimeSeconds"); LOC(fogEnabled, "fogEnabled");
    LOC(fogColor, "fogColor"); LOC(fogStartDistance, "fogStartDistance");
    LOC(fogDensity, "fogDensity"); LOC(fogMaximumOpacity, "fogMaximumOpacity");
    LOC(fogReferenceHeight, "fogReferenceHeight"); LOC(fogHeightFalloff, "fogHeightFalloff");
    LOC(localVolumeCount, "localVolumeCount");
    LOC(shadowMap0, "shadowMap0"); LOC(shadowMap1, "shadowMap1");
#undef LOC
    locations.localCenters = ArrayLocation(shader, "localCenters");
    locations.localRadii = ArrayLocation(shader, "localRadii");
    locations.localColors = ArrayLocation(shader, "localColors");
    locations.localParamsA = ArrayLocation(shader, "localParamsA");
    locations.localParamsB = ArrayLocation(shader, "localParamsB");
    locations.dynamicLights.dynamicLightCount = GetShaderLocation(shader, "dynamicLightCount");
    locations.dynamicLights.dynamicLightPositions = ArrayLocation(shader, "dynamicLightPositions");
    locations.dynamicLights.dynamicLightColors = ArrayLocation(shader, "dynamicLightColors");
    locations.dynamicLights.dynamicLightRadii = ArrayLocation(shader, "dynamicLightRadii");
    locations.dynamicLights.dynamicLightIntensities = ArrayLocation(shader, "dynamicLightIntensities");
    locations.dynamicLights.dynamicLightTypes = ArrayLocation(shader, "dynamicLightTypes");
    locations.dynamicLights.dynamicLightDirections = ArrayLocation(shader, "dynamicLightDirections");
    locations.dynamicLights.dynamicLightInnerConeCos = ArrayLocation(shader, "dynamicLightInnerConeCos");
    locations.dynamicLights.dynamicLightOuterConeCos = ArrayLocation(shader, "dynamicLightOuterConeCos");
    locations.dynamicShadows.dynamicLightShadowSlots = ArrayLocation(shader, "dynamicLightShadowSlots");
    for (std::size_t index = 0; index < MaxDynamicSpotLightShadowCasters; ++index) {
        locations.dynamicShadows.shadowLightMatrices[index] =
                ArrayElementLocation(shader, "shadowLightMatrices", index);
    }
    locations.dynamicShadows.shadowBias = ArrayLocation(shader, "shadowBias");
    locations.dynamicShadows.shadowStrength = ArrayLocation(shader, "shadowStrength");
    locations.dynamicShadows.shadowSoftness = ArrayLocation(shader, "shadowSoftness");
    compositeLocations.sceneColor = GetShaderLocation(compositeShader, "sceneColor");
    compositeLocations.sceneDepth = GetShaderLocation(compositeShader, "sceneDepth");
    compositeLocations.atmosphereTexture = GetShaderLocation(compositeShader, "atmosphereTexture");
    compositeLocations.atmosphereTexelSize = GetShaderLocation(compositeShader, "atmosphereTexelSize");
    resourceDiagnostic = "shader ready; target not allocated";
    return true;
}

bool SectorVolumetricAtmosphereRenderer::EnsureTargets(int width, int height)
{
    if (engine::IsRenderTargetReady(target)
            && target.native.texture.width == width
            && target.native.texture.height == height) return true;
    if (failedWidth == width && failedHeight == height) return false;
    ReleaseTargets();
    std::string error;
    engine::LoadRenderTarget(
            engine::RenderTargetDescriptor{
                    "unified-atmosphere-accumulation", width, height,
                    engine::RenderTargetColorFormat::Rgba16Float,
                    engine::RenderTargetFilter::Bilinear,
                    engine::RenderTargetWrap::Clamp,
                    engine::RenderTargetDepthKind::None, 1},
            target, &error);
    if (!engine::IsRenderTargetReady(target)) {
        failedWidth = width;
        failedHeight = height;
        resourceDiagnostic = "disabled: " + error;
        return false;
    }
    failedWidth = 0;
    failedHeight = 0;
    resourceDiagnostic = engine::FormatRenderTargetDiagnostic(target);
    return true;
}

void SectorVolumetricAtmosphereRenderer::ReleaseTargets()
{
    engine::UnloadRenderTarget(target);
}

void SectorVolumetricAtmosphereRenderer::BuildLocalVolumes(
        const SectorTopologyMap& map,
        SectorTopologyFogSettings::VolumetricQuality quality,
        const Camera3D& camera)
{
    localVolumes = {};
    eligibleLocalVolumeCount = 0;
    activeLocalVolumeCount = 0;
    const int cap = std::min<int>(
            GetSectorLegacyAtmosphereQualityContract(quality).maximumLocalFogVolumes,
            static_cast<int>(MaximumLocalVolumes));
    if (cap <= 0) return;
    std::array<const SectorCompiledLocalFogVolume*, MaximumLocalVolumes> selected{};
    std::array<float, MaximumLocalVolumes> distances{};
    distances.fill(std::numeric_limits<float>::max());
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    for (const SectorCompiledLocalFogVolume& volume : map.compiledLocalFogVolumes) {
        if (!volume.enabled || volume.density <= 0.0f || volume.maxOpacity <= 0.0f) continue;
        const Vector3 toCenter = Vector3Subtract(volume.centerWorld, camera.position);
        const float maximumRadius = std::max({
                volume.radiiWorld.x, volume.radiiWorld.y, volume.radiiWorld.z});
        if (Vector3DotProduct(toCenter, forward) < -maximumRadius) continue;
        ++eligibleLocalVolumeCount;
        const float distance = Vector3LengthSqr(toCenter);
        int insertAt = std::min(activeLocalVolumeCount, cap - 1);
        const auto comesBefore = [&](int index) {
            const SectorCompiledLocalFogVolume* existing =
                    selected[static_cast<std::size_t>(index)];
            return distance < distances[static_cast<std::size_t>(index)]
                    || (distance == distances[static_cast<std::size_t>(index)]
                        && existing != nullptr
                        && volume.sourceAuthoringFogVolumeId
                                < existing->sourceAuthoringFogVolumeId);
        };
        if (activeLocalVolumeCount >= cap && !comesBefore(cap - 1)) continue;
        if (activeLocalVolumeCount < cap) ++activeLocalVolumeCount;
        while (insertAt > 0 && comesBefore(insertAt - 1)) {
            selected[static_cast<std::size_t>(insertAt)] =
                    selected[static_cast<std::size_t>(insertAt - 1)];
            distances[static_cast<std::size_t>(insertAt)] =
                    distances[static_cast<std::size_t>(insertAt - 1)];
            --insertAt;
        }
        selected[static_cast<std::size_t>(insertAt)] = &volume;
        distances[static_cast<std::size_t>(insertAt)] = distance;
    }
    for (int index = 0; index < activeLocalVolumeCount; ++index) {
        const SectorCompiledLocalFogVolume& volume =
                *selected[static_cast<std::size_t>(index)];
        LocalVolume& record =
                localVolumes[static_cast<std::size_t>(index)];
        record.stableId = volume.sourceAuthoringFogVolumeId;
        record.centerWorld = volume.centerWorld;
        record.radiiWorld = volume.radiiWorld;
        record.tint = engine::SrgbColorBytesToLinearSceneRgb(volume.color);
        record.density = volume.density;
        record.maximumOpacity = volume.maxOpacity;
        record.edgeSoftness = volume.edgeSoftness;
        record.noiseAmount = volume.noiseAmount;
        record.noiseScaleWorld = volume.noiseScaleWorld;
        record.flowDirectionRadians = volume.flowDirectionDegrees * DEG2RAD;
        record.flowSpeedWorld = volume.flowSpeedWorld;
    }
}

void SectorVolumetricAtmosphereRenderer::BuildLightContext(
        const SectorBillboardDynamicLightContext& dynamicLights)
{
    dynamicLightContext = {};
    dynamicLightContext.dynamicLightCount = lightSelection.activeCount;
    dynamicLightContext.shadowMaps = dynamicLights.shadowMaps;
    dynamicLightContext.shadowUniforms = dynamicLights.shadowUniforms;
    dynamicLightContext.shadowUniforms.dynamicLightShadowSlots.fill(-1);
    for (int index = 0; index < lightSelection.activeCount; ++index) {
        const SectorVolumetricLightRecord& light =
                lightSelection.lights[static_cast<std::size_t>(index)];
        const bool spot = light.kind == SectorLightAtmosphereSourceKind::StaticSpot
                || light.kind == SectorLightAtmosphereSourceKind::DynamicSpot;
        dynamicLightContext.dynamicLightIds[static_cast<std::size_t>(index)] =
                light.lightId;
        dynamicLightContext.dynamicLightPositions[static_cast<std::size_t>(index)] =
                light.positionWorld;
        dynamicLightContext.dynamicLightColors[static_cast<std::size_t>(index)] =
                engine::SrgbColorBytesToLinearSceneRgb(light.color);
        dynamicLightContext.dynamicLightRadii[static_cast<std::size_t>(index)] =
                light.rangeWorld;
        dynamicLightContext.dynamicLightIntensities[static_cast<std::size_t>(index)] =
                light.effectiveIntensity * (light.flicker
                        ? EvaluateDynamicLightFlickerMultiplier(
                                light.lightId,
                                preparedRuntimeSeconds,
                                light.flickerSpeed,
                                light.flickerAmount)
                        : 1.0f);
        dynamicLightContext.dynamicLightTypes[static_cast<std::size_t>(index)] =
                spot ? 1 : 0;
        dynamicLightContext.dynamicLightDirections[static_cast<std::size_t>(index)] =
                light.directionWorld;
        dynamicLightContext.dynamicLightInnerConeCos[static_cast<std::size_t>(index)] =
                light.innerConeCos;
        dynamicLightContext.dynamicLightOuterConeCos[static_cast<std::size_t>(index)] =
                light.outerConeCos;
        if (light.kind != SectorLightAtmosphereSourceKind::DynamicSpot) continue;
        for (int dynamicIndex = 0;
             dynamicIndex < dynamicLights.dynamicLightCount;
             ++dynamicIndex) {
            if (dynamicLights.dynamicLightIds[static_cast<std::size_t>(dynamicIndex)]
                            == light.lightId
                    && dynamicLights.dynamicLightTypes[
                               static_cast<std::size_t>(dynamicIndex)] == 1) {
                dynamicLightContext.shadowUniforms.dynamicLightShadowSlots[
                        static_cast<std::size_t>(index)] =
                        dynamicLights.shadowUniforms.dynamicLightShadowSlots[
                                static_cast<std::size_t>(dynamicIndex)];
                break;
            }
        }
    }
}

bool SectorVolumetricAtmosphereRenderer::Prepare(
        const engine::RenderTarget& sceneTarget,
        const SectorTopologyMap& map,
        SectorTopologyFogSettings::VolumetricQuality quality,
        const Camera3D& camera,
        float runtimeSeconds,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& lightAtmosphereSources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        bool dynamicLightingEnabled)
{
    ResetPreparedFrame();
    prepared = true;
    fogSettings = NormalizeSectorTopologyFogSettings(map.fogSettings);
    globalFogActive = fogSettings.enabled && fogSettings.density > 0.0f
            && fogSettings.maxOpacity > 0.0f;
    preparedCamera = camera;
    preparedRuntimeSeconds = runtimeSeconds;
    nearPlane = static_cast<float>(rlGetCullDistanceNear());
    farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (quality == SectorTopologyFogSettings::VolumetricQuality::Off
            || sceneTarget.descriptor.colorFormat
                    != engine::RenderTargetColorFormat::Rgba16Float
            || sceneTarget.actual.depth
                    != engine::RenderTargetDepthKind::SampleableTexture
            || !std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane) {
        resourceDiagnostic = quality == SectorTopologyFogSettings::VolumetricQuality::Off
                ? "inactive: volumetric quality Off"
                : "disabled: unsupported target or camera projection";
        return false;
    }
    const SectorVolumetricGridSize grid = ComputeSectorVolumetricGridSize(
            quality,
            sceneTarget.native.texture.width,
            sceneTarget.native.texture.height);
    marchSteps = grid.z;
    aspectRatio = static_cast<float>(sceneTarget.native.texture.width)
            / std::max(sceneTarget.native.texture.height, 1);
    BuildLocalVolumes(map, quality, camera);
    lightSelection = SelectSectorTemporaryVolumetricLights(
            lightAtmosphereSources,
            visibility,
            receiverBounds,
            camera,
            aspectRatio,
            nearPlane,
            std::min(farPlane, fogSettings.volumetricMaxDistanceWorld),
            dynamicLightingEnabled);
    BuildLightContext(dynamicLights);
    activeMedia = globalFogActive || activeLocalVolumeCount > 0;
    if (!activeMedia) {
        resourceDiagnostic = "inactive: no visible unified media";
        return false;
    }
    resourcesReady = Initialize()
            && EnsureTargets(std::max(grid.x, 1), std::max(grid.y, 1));
    if (!resourcesReady && !warnedUnavailable) {
        TraceLog(LOG_WARNING,
                "UNIFIED ATMOSPHERE: resources unavailable; using analytic fog fallback");
        warnedUnavailable = true;
    }
    return resourcesReady;
}

bool SectorVolumetricAtmosphereRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& sceneScratch)
{
    if (!prepared || !resourcesReady || !activeMedia
            || sceneTarget.texture.id == 0 || sceneTarget.depth.id == 0) {
        return false;
    }
    std::array<Vector3, MaximumLocalVolumes> localCenters{};
    std::array<Vector3, MaximumLocalVolumes> localRadii{};
    std::array<Vector3, MaximumLocalVolumes> localColors{};
    std::array<Vector4, MaximumLocalVolumes> localParamsA{};
    std::array<Vector4, MaximumLocalVolumes> localParamsB{};
    for (int index = 0; index < activeLocalVolumeCount; ++index) {
        const auto& record = localVolumes[static_cast<std::size_t>(index)];
        localCenters[static_cast<std::size_t>(index)] = record.centerWorld;
        localRadii[static_cast<std::size_t>(index)] = record.radiiWorld;
        localColors[static_cast<std::size_t>(index)] = record.tint;
        localParamsA[static_cast<std::size_t>(index)] = Vector4{
                record.density, record.maximumOpacity, record.edgeSoftness,
                record.noiseScaleWorld};
        localParamsB[static_cast<std::size_t>(index)] = Vector4{
                record.noiseAmount, record.flowDirectionRadians,
                record.flowSpeedWorld, 0.0f};
    }
    Vector3 forward = Vector3Normalize(Vector3Subtract(
            preparedCamera.target, preparedCamera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, preparedCamera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float tanHalfFov = std::tan(preparedCamera.fovy * DEG2RAD * 0.5f);
    const float maximumDistance = fogSettings.volumetricMaxDistanceWorld;
    const float anisotropy = fogSettings.anisotropy;
    const int fogEnabled = globalFogActive ? 1 : 0;
    const Vector3 fogColor = engine::SrgbColorBytesToLinearSceneRgb(fogSettings.color);

    rlDrawRenderBatchActive();
    BeginTextureMode(target.native);
    ClearBackground(BLANK);
    BeginShaderMode(shader);
    SetShaderValueTexture(shader, locations.sceneDepth, sceneTarget.depth);
    if (locations.shadowMap0 >= 0 && dynamicLightContext.shadowMaps.shadowMap0 != nullptr
            && dynamicLightContext.shadowMaps.shadowMap0->id != 0) {
        SetShaderValueTexture(shader, locations.shadowMap0,
                *dynamicLightContext.shadowMaps.shadowMap0);
    }
    if (locations.shadowMap1 >= 0 && dynamicLightContext.shadowMaps.shadowMap1 != nullptr
            && dynamicLightContext.shadowMaps.shadowMap1->id != 0) {
        SetShaderValueTexture(shader, locations.shadowMap1,
                *dynamicLightContext.shadowMaps.shadowMap1);
    }
#define SET(field, value, type) SetShaderValue(shader, locations.field, &value, type)
    SET(marchSteps, marchSteps, SHADER_UNIFORM_INT);
    SET(cameraPosition, preparedCamera.position, SHADER_UNIFORM_VEC3);
    SET(cameraForward, forward, SHADER_UNIFORM_VEC3);
    SET(cameraRight, right, SHADER_UNIFORM_VEC3); SET(cameraUp, up, SHADER_UNIFORM_VEC3);
    SET(tanHalfFov, tanHalfFov, SHADER_UNIFORM_FLOAT); SET(aspectRatio, aspectRatio, SHADER_UNIFORM_FLOAT);
    SET(nearPlane, nearPlane, SHADER_UNIFORM_FLOAT); SET(farPlane, farPlane, SHADER_UNIFORM_FLOAT);
    SET(maximumDistance, maximumDistance, SHADER_UNIFORM_FLOAT); SET(anisotropy, anisotropy, SHADER_UNIFORM_FLOAT);
    SET(runtimeSeconds, preparedRuntimeSeconds, SHADER_UNIFORM_FLOAT);
    SET(fogEnabled, fogEnabled, SHADER_UNIFORM_INT); SET(fogColor, fogColor, SHADER_UNIFORM_VEC3);
    SET(fogStartDistance, fogSettings.startDistanceWorld, SHADER_UNIFORM_FLOAT);
    SET(fogDensity, fogSettings.density, SHADER_UNIFORM_FLOAT);
    SET(fogMaximumOpacity, fogSettings.maxOpacity, SHADER_UNIFORM_FLOAT);
    SET(fogReferenceHeight, fogSettings.referenceHeightWorld, SHADER_UNIFORM_FLOAT);
    SET(fogHeightFalloff, fogSettings.heightFalloff, SHADER_UNIFORM_FLOAT);
    SET(localVolumeCount, activeLocalVolumeCount, SHADER_UNIFORM_INT);
#undef SET
    SetShaderValueV(shader, locations.localCenters, localCenters.data(), SHADER_UNIFORM_VEC3, activeLocalVolumeCount);
    SetShaderValueV(shader, locations.localRadii, localRadii.data(), SHADER_UNIFORM_VEC3, activeLocalVolumeCount);
    SetShaderValueV(shader, locations.localColors, localColors.data(), SHADER_UNIFORM_VEC3, activeLocalVolumeCount);
    SetShaderValueV(shader, locations.localParamsA, localParamsA.data(), SHADER_UNIFORM_VEC4, activeLocalVolumeCount);
    SetShaderValueV(shader, locations.localParamsB, localParamsB.data(), SHADER_UNIFORM_VEC4, activeLocalVolumeCount);
    UploadSectorRendererDynamicPointLights(shader, locations.dynamicLights, dynamicLightContext);
    UploadSectorRendererDynamicSpotLightShadowUniforms(
            shader, locations.dynamicShadows, dynamicLightContext.shadowUniforms);
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture),
            DestinationRect(target.native.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

    const Vector2 texelSize{
            1.0f / target.native.texture.width,
            1.0f / target.native.texture.height};
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneScratch);
    ClearBackground(BLANK);
    BeginShaderMode(compositeShader);
    SetShaderValueTexture(compositeShader, compositeLocations.sceneColor, sceneTarget.texture);
    SetShaderValueTexture(compositeShader, compositeLocations.sceneDepth, sceneTarget.depth);
    SetShaderValueTexture(compositeShader, compositeLocations.atmosphereTexture, target.native.texture);
    SetShaderValue(compositeShader, compositeLocations.atmosphereTexelSize,
            &texelSize, SHADER_UNIFORM_VEC2);
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture),
            DestinationRect(sceneScratch.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();
    return true;
}

void SectorVolumetricAtmosphereRenderer::ResetPreparedFrame()
{
    prepared = false;
    resourcesReady = false;
    activeMedia = false;
    globalFogActive = false;
    marchSteps = 0;
    eligibleLocalVolumeCount = 0;
    activeLocalVolumeCount = 0;
    localVolumes = {};
    lightSelection = {};
    dynamicLightContext = {};
}

void SectorVolumetricAtmosphereRenderer::Shutdown()
{
    ReleaseTargets();
    if (shader.id != 0) UnloadShader(shader);
    if (compositeShader.id != 0) UnloadShader(compositeShader);
    shader = {};
    compositeShader = {};
    locations = {};
    compositeLocations = {};
    failedWidth = 0;
    failedHeight = 0;
    shaderFailed = false;
    warnedUnavailable = false;
    resourceDiagnostic = "not initialized";
    ResetPreparedFrame();
}

} // namespace game
