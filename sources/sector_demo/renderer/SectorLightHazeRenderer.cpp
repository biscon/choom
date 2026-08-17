#include "sector_demo/renderer/SectorLightHazeRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/renderer/SectorFog.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <string>

namespace game {
namespace {

constexpr int MaxHazeVolumes = 8;
constexpr float HazeMaximumOpacity = 0.30f;
constexpr SectorLocalFogPathLimitSettings HazePathLimitSettings{};

const char* FullscreenVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragUv;
uniform mat4 mvp;
void main() { fragUv = vertexTexCoord; gl_Position = mvp * vec4(vertexPosition, 1.0); }
)";

const char* HazeFs = R"(
#version 330
in vec2 fragUv;
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
float StoreBoundedAlpha(float value) {
    return (isnan(value) || isinf(value)) ? 0.0 : clamp(value, 0.0, 1.0);
}
uniform sampler2D sceneDepth;
uniform int volumeCount;
uniform int marchSteps;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform float nearPlane;
uniform float farPlane;
uniform float runtimeSeconds;
uniform float pathMinimumWorld;
uniform float pathThicknessMultiplier;
uniform float pathSaturationPower;
uniform vec3 hazeCenters[8];
uniform vec3 hazeDirections[8];
uniform vec3 hazeBoundsCenters[8];
uniform float hazeBoundsRadii[8];
uniform int hazeShapes[8];
uniform float hazeExtents[8];
uniform float hazeConeRadii[8];
uniform vec3 hazeColors[8];
uniform vec4 hazeParamsA[8];
uniform vec4 hazeParamsB[8];
uniform int hazeOwnerDynamicLightIndices[8];
uniform vec3 hazeStaticLighting[64];

uniform int fogEnabled;
uniform vec3 fogColor;
uniform float fogStartDistance;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeight;
uniform float fogHeightFalloff;

#define MAX_DYNAMIC_LIGHTS 8
#define MAX_DYNAMIC_SHADOW_CASTERS 2
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

const vec2 kShadowDisk[4] = vec2[4](
    vec2(-0.707, -0.707), vec2(0.707, -0.707),
    vec2(-0.707, 0.707), vec2(0.707, 0.707));

vec3 safeNormalize(vec3 v, vec3 fallback) {
    float l2 = dot(v, v); return l2 > 0.00000001 ? v * inversesqrt(l2) : fallback;
}
float hash31(vec3 p) {
    p = fract(p * 0.1031); p += dot(p, p.yzx + 33.33); return fract((p.x + p.y) * p.z);
}
float valueNoise(vec3 p) {
    vec3 c = floor(p); vec3 f = fract(p); f = f*f*(3.0-2.0*f);
    return mix(mix(mix(hash31(c),hash31(c+vec3(1,0,0)),f.x),
                   mix(hash31(c+vec3(0,1,0)),hash31(c+vec3(1,1,0)),f.x),f.y),
               mix(mix(hash31(c+vec3(0,0,1)),hash31(c+vec3(1,0,1)),f.x),
                   mix(hash31(c+vec3(0,1,1)),hash31(c+vec3(1,1,1)),f.x),f.y),f.z);
}
bool intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius, out float t0, out float t1) {
    vec3 oc = ro-center; float b=dot(oc,rd); float c=dot(oc,oc)-radius*radius;
    float h=b*b-c; if(h<0.0) return false; h=sqrt(h); t0=-b-h; t1=-b+h; return t1>max(t0,0.0);
}
void shapeSample(int i, vec3 p, out bool inside, out float boundary, out vec3 grid) {
    vec3 axis=safeNormalize(hazeDirections[i],vec3(0,-1,0));
    vec3 reference=abs(axis.y)>0.98?vec3(0,0,1):vec3(0,1,0);
    vec3 right=safeNormalize(cross(axis,reference),vec3(1,0,0));
    vec3 up=safeNormalize(cross(right,axis),vec3(0,1,0));
    vec3 q=p-hazeCenters[i]; float softness=max(hazeParamsA[i].z,0.01);
    if(hazeShapes[i]==0) {
        vec3 n=q/max(hazeExtents[i],0.0001); float r=length(n); inside=r<=1.0;
        boundary=1.0-smoothstep(1.0-softness,1.0,r);
        grid=vec3(dot(q,right),dot(q,up),dot(q,axis))/max(hazeExtents[i],0.0001)+0.5;
    } else {
        float axial=dot(q,axis); float axial01=axial/max(hazeExtents[i],0.0001);
        vec3 radial=q-axis*axial; float allowed=hazeConeRadii[i]*max(axial01,0.0001);
        float radial01=length(radial)/max(allowed,0.0001);
        inside=axial01>=0.0&&axial01<=1.0&&radial01<=1.0;
        float side=1.0-smoothstep(1.0-softness,1.0,radial01);
        float endFade=1.0-smoothstep(1.0-softness,1.0,axial01);
        float startFade=smoothstep(0.0,max(softness*0.25,0.01),axial01);
        boundary=side*endFade*startFade;
        grid=vec3(dot(radial,right)/max(allowed,0.0001)+0.5,
                  dot(radial,up)/max(allowed,0.0001)+0.5,
                  (axial01-0.25)/0.5);
    }
}
float shadowDepth(int slot, vec2 uv) { return slot==0?texture(shadowMap0,uv).r:texture(shadowMap1,uv).r; }
float shadowVisibility(int slot, vec3 p) {
    if(slot<0||slot>=2) return 1.0; vec4 clip=shadowLightMatrices[slot]*vec4(p,1);
    if(clip.w<=0.0) return 1.0; vec3 sc=clip.xyz/clip.w*0.5+0.5;
    if(any(lessThan(sc,vec3(0)))||any(greaterThan(sc,vec3(1)))) return 1.0;
    float compare=sc.z-min(max(shadowBias[slot],0.0),0.02); float soft=clamp(shadowSoftness[slot],0.0,8.0);
    if(soft<=0.0) return compare<=shadowDepth(slot,sc.xy)?1.0:0.0;
    vec2 texel=1.0/vec2(textureSize(shadowMap0,0)); float visible=0.0;
    for(int j=0;j<4;++j) visible+=compare<=shadowDepth(slot,clamp(sc.xy+kShadowDisk[j]*max(0.25,soft)*texel,vec2(0),vec2(1)))?1.0:0.0;
    return visible*0.25;
}
vec3 dynamicLighting(vec3 p, int volumeIndex, float heightOffsetWorld) {
    vec3 result=vec3(0); for(int i=0;i<MAX_DYNAMIC_LIGHTS;++i) { if(i>=dynamicLightCount) break;
        vec3 lightingPosition=i==hazeOwnerDynamicLightIndices[volumeIndex]
                ?p-vec3(0,heightOffsetWorld,0):p;
        float radius=dynamicLightRadii[i]; vec3 toLight=dynamicLightPositions[i]-lightingPosition; float d2=dot(toLight,toLight);
        if(radius<=0.0||d2>=radius*radius) continue; float d=sqrt(max(d2,0.0));
        vec3 lightDir=d>0.0001?toLight/d:vec3(0,1,0); float atten=clamp(1.0-d/radius,0.0,1.0); atten*=atten;
        float cone=1.0; if(dynamicLightTypes[i]==1) { vec3 sd=safeNormalize(dynamicLightDirections[i],vec3(0,-1,0));
            float cd=dot(sd,d>0.0001?-lightDir:sd); float inner=dynamicLightInnerConeCos[i], outer=dynamicLightOuterConeCos[i];
            cone=abs(inner-outer)>0.0001?smoothstep(outer,inner,cd):step(inner,cd); int slot=dynamicLightShadowSlots[i];
            if(slot>=0&&cone>0.0) cone*=mix(1.0,shadowVisibility(slot,lightingPosition),clamp(shadowStrength[slot],0.0,1.0)); }
        result+=dynamicLightColors[i]*dynamicLightIntensities[i]*atten*cone; }
    return result;
}
vec3 staticLighting(int volumeIndex, vec3 grid) {
    vec3 g=clamp(grid,vec3(0),vec3(1)); int b=volumeIndex*8;
    vec3 z0y0=mix(hazeStaticLighting[b],hazeStaticLighting[b+1],g.x);
    vec3 z0y1=mix(hazeStaticLighting[b+2],hazeStaticLighting[b+3],g.x);
    vec3 z1y0=mix(hazeStaticLighting[b+4],hazeStaticLighting[b+5],g.x);
    vec3 z1y1=mix(hazeStaticLighting[b+6],hazeStaticLighting[b+7],g.x);
    return mix(mix(z0y0,z0y1,g.y),mix(z1y0,z1y1,g.y),g.z);
}
float effectivePath(float geometric, float thickness) {
    float limit=max(pathMinimumWorld,max(thickness,0.0)*pathThicknessMultiplier);
    float power=max(pathSaturationPower,1.0); float ratio=geometric/limit;
    return min(geometric/max(pow(1.0+pow(ratio,power),1.0/power),1.0),limit);
}
float distanceFogTransmittance(vec3 p) {
    if(fogEnabled==0||fogDensity<=0.0||fogMaxOpacity<=0.0) return 1.0;
    float distance=max(length(p-cameraPosition)-fogStartDistance,0.0);
    float midpoint=(cameraPosition.y+p.y)*0.5; float above=max(midpoint-fogReferenceHeight,0.0);
    float amount=min(1.0-exp(-fogDensity*distance*exp(-above*fogHeightFalloff)),fogMaxOpacity);
    return 1.0-amount;
}
void main() {
    float depth=texture(sceneDepth,fragUv).r; vec2 ndc=fragUv*2.0-1.0;
    vec3 rd=normalize(cameraForward+cameraRight*ndc.x*tanHalfFov*aspectRatio+cameraUp*ndc.y*tanHalfFov);
    float z=depth*2.0-1.0; float forwardDistance=(2.0*nearPlane*farPlane)/max(farPlane+nearPlane-z*(farPlane-nearPlane),0.00001);
    float sceneDistance=depth>=0.999999?farPlane:forwardDistance/max(dot(rd,cameraForward),0.0001);
    float totalDepth=0.0; vec3 weighted=vec3(0);
    for(int i=0;i<8;++i) { if(i>=volumeCount) break; float enter,exit;
        if(!intersectSphere(cameraPosition,rd,hazeBoundsCenters[i],hazeBoundsRadii[i],enter,exit)) continue;
        enter=max(enter,0.0); exit=min(exit,sceneDistance); if(exit<=enter) continue;
        float segment=exit-enter; float geometricStep=segment/float(max(marchSteps,1)); int insideCount=0;
        for(int s=0;s<12;++s) { if(s>=marchSteps) break; bool inside; float boundary; vec3 grid;
            vec3 p=cameraPosition+rd*(enter+(float(s)+0.5)*geometricStep);
            shapeSample(i,p,inside,boundary,grid); if(inside) insideCount++; }
        if(insideCount==0) continue; float insideLength=geometricStep*float(insideCount);
        float thickness=hazeShapes[i]==0?hazeExtents[i]*2.0:max(hazeConeRadii[i]*2.0,0.1);
        float opticalStep=effectivePath(insideLength,thickness)/float(insideCount);
        float volumeDepth=0.0; vec3 volumeWeighted=vec3(0); vec4 a=hazeParamsA[i], b=hazeParamsB[i];
        vec2 flowWorld=vec2(cos(b.y),sin(b.y))*b.z*runtimeSeconds;
        float inverseNoiseScale=1.0/max(a.w,0.05);
        for(int s=0;s<12;++s) { if(s>=marchSteps) break; vec3 p=cameraPosition+rd*(enter+(float(s)+0.5)*geometricStep);
            bool inside; float boundary; vec3 grid; shapeSample(i,p,inside,boundary,grid); if(!inside) continue;
            float noiseModulation=1.0;
            if(b.x>0.0001) { vec3 noiseSamplePosition=p; noiseSamplePosition.xz-=flowWorld;
                float coherentNoise=smoothstep(0.15,0.85,valueNoise(noiseSamplePosition*inverseNoiseScale));
                noiseModulation=mix(1.0,2.0*coherentNoise,b.x); }
            float sampleDepth=a.x*boundary*noiseModulation*opticalStep; if(sampleDepth<=0.0) continue;
            vec3 lighting=max(staticLighting(i,grid)+dynamicLighting(p,i,b.w),vec3(0));
            volumeDepth+=sampleDepth; volumeWeighted+=hazeColors[i]*lighting*distanceFogTransmittance(p)*sampleDepth; }
        float cap=-log(max(1.0-clamp(a.y,0.0,0.9999),0.0001)); float capped=min(volumeDepth,cap);
        float scale=volumeDepth>0.00001?capped/volumeDepth:0.0; totalDepth+=capped; weighted+=volumeWeighted*scale;
    }
    float opacity=1.0-exp(-totalDepth); vec3 color=totalDepth>0.00001?weighted/totalDepth:vec3(0);
    finalColor=vec4(StoreFiniteHalfRadiance(color*opacity),StoreBoundedAlpha(opacity));
}
)";

const char* CompositeFs = R"(
#version 330
in vec2 fragUv; out vec4 finalColor;
uniform sampler2D sceneColor; uniform sampler2D sceneDepth; uniform sampler2D hazeTexture;
uniform vec2 hazeTexelSize; uniform int bilateralUpsample;
float SanitizeIntermediateChannel(float value) {
    if(isnan(value)) return 0.0;
    if(isinf(value)) return value>0.0?65504.0:0.0;
    return min(max(value,0.0),65504.0);
}
vec3 SanitizeIntermediateRadiance(vec3 value) {
    return vec3(SanitizeIntermediateChannel(value.r),SanitizeIntermediateChannel(value.g),
            SanitizeIntermediateChannel(value.b));
}
float SanitizeOpacity(float value) {
    return (isnan(value)||isinf(value))?0.0:clamp(value,0.0,1.0);
}
void main() {
    vec4 haze=texture(hazeTexture,fragUv);
    if(bilateralUpsample!=0) { float center=texture(sceneDepth,fragUv).r; vec4 sum=vec4(0); float weightSum=0.0;
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) { if(x!=0&&y!=0) continue; vec2 uv=clamp(fragUv+vec2(x,y)*hazeTexelSize,vec2(0),vec2(1));
            float weight=exp(-abs(texture(sceneDepth,uv).r-center)*600.0); sum+=texture(hazeTexture,uv)*weight; weightSum+=weight; }
        haze=sum/max(weightSum,0.0001); }
    vec4 scene=texture(sceneColor,fragUv);
    vec3 composed=SanitizeIntermediateRadiance(scene.rgb)*(1.0-SanitizeOpacity(haze.a))
            +SanitizeIntermediateRadiance(haze.rgb);
    float sceneAlpha=(isnan(scene.a)||isinf(scene.a))?1.0:clamp(scene.a,0.0,1.0);
    finalColor=vec4(SanitizeIntermediateRadiance(composed),sceneAlpha);
}
)";

Rectangle Src(Texture2D t) { return Rectangle{0,0,static_cast<float>(t.width),-static_cast<float>(t.height)}; }
Rectangle Dst(Texture2D t) { return Rectangle{0,0,static_cast<float>(t.width),static_cast<float>(t.height)}; }
bool Ready(Shader s) { return s.id != 0; }
int ArrayLoc(Shader s, const char* name) {
    int location=GetShaderLocation(s,name); if(location>=0) return location;
    const std::string indexed=std::string(name)+"[0]"; return GetShaderLocation(s,indexed.c_str());
}
int ArrayElementLoc(Shader s, const char* name, std::size_t index) {
    const std::string indexed=std::string(name)+"["+std::to_string(index)+"]";
    return GetShaderLocation(s,indexed.c_str());
}
bool SameVector(Vector3 a, Vector3 b) { return a.x==b.x&&a.y==b.y&&a.z==b.z; }

SectorLightAtmosphereVolume UndisplacedLightingVolume(
        SectorLightAtmosphereVolume volume,
        float heightOffsetWorld)
{
    volume.originWorld.y -= heightOffsetWorld;
    volume.boundsCenterWorld.y -= heightOffsetWorld;
    return volume;
}

int FindOwnerDynamicLightIndex(
        const SectorLightAtmosphereSource& source,
        const SectorBillboardDynamicLightContext& dynamicLights)
{
    if (!IsSectorLightAtmosphereSourceDynamic(source)) return -1;
    const int expectedType = source.kind == SectorLightAtmosphereSourceKind::DynamicSpot ? 1 : 0;
    for (int index = 0; index < dynamicLights.dynamicLightCount; ++index) {
        if (dynamicLights.dynamicLightIds[static_cast<std::size_t>(index)] == source.lightId
                && dynamicLights.dynamicLightTypes[static_cast<std::size_t>(index)] == expectedType) {
            return index;
        }
    }
    return -1;
}

} // namespace

bool SectorLightHazeRenderer::EnsureShaders()
{
    if (Ready(shader) && Ready(compositeShader)) return true;
    if(shaderFailed) return false;
    Shutdown();
    shader=LoadShaderFromMemory(FullscreenVs,HazeFs); compositeShader=LoadShaderFromMemory(FullscreenVs,CompositeFs);
    if(!Ready(shader)||!Ready(compositeShader)) {
        shaderFailed=true;
        accumulationDiagnostic="disabled: shader unavailable";
        return false;
    }
#define LOC(field, name) locations.field=GetShaderLocation(shader,name)
    LOC(sceneDepth,"sceneDepth"); LOC(volumeCount,"volumeCount"); LOC(marchSteps,"marchSteps");
    LOC(cameraPosition,"cameraPosition"); LOC(cameraForward,"cameraForward"); LOC(cameraRight,"cameraRight");
    LOC(cameraUp,"cameraUp"); LOC(tanHalfFov,"tanHalfFov"); LOC(aspectRatio,"aspectRatio");
    LOC(nearPlane,"nearPlane"); LOC(farPlane,"farPlane"); LOC(runtimeSeconds,"runtimeSeconds");
    LOC(pathMinimumWorld,"pathMinimumWorld"); LOC(pathThicknessMultiplier,"pathThicknessMultiplier");
    LOC(pathSaturationPower,"pathSaturationPower");
    locations.centers=ArrayLoc(shader,"hazeCenters"); locations.directions=ArrayLoc(shader,"hazeDirections");
    locations.boundsCenters=ArrayLoc(shader,"hazeBoundsCenters"); locations.boundsRadii=ArrayLoc(shader,"hazeBoundsRadii");
    locations.shapes=ArrayLoc(shader,"hazeShapes"); locations.extents=ArrayLoc(shader,"hazeExtents");
    locations.coneRadii=ArrayLoc(shader,"hazeConeRadii"); locations.colors=ArrayLoc(shader,"hazeColors");
    locations.paramsA=ArrayLoc(shader,"hazeParamsA"); locations.paramsB=ArrayLoc(shader,"hazeParamsB");
    locations.ownerDynamicLightIndices=ArrayLoc(shader,"hazeOwnerDynamicLightIndices");
    locations.staticLighting=ArrayLoc(shader,"hazeStaticLighting");
    LOC(fogEnabled,"fogEnabled"); LOC(fogColor,"fogColor"); LOC(fogStartDistance,"fogStartDistance");
    LOC(fogDensity,"fogDensity"); LOC(fogMaxOpacity,"fogMaxOpacity"); LOC(fogReferenceHeight,"fogReferenceHeight");
    LOC(fogHeightFalloff,"fogHeightFalloff");
#undef LOC
    locations.dynamicLights.dynamicLightCount=GetShaderLocation(shader,"dynamicLightCount");
    locations.dynamicLights.dynamicLightPositions=ArrayLoc(shader,"dynamicLightPositions");
    locations.dynamicLights.dynamicLightColors=ArrayLoc(shader,"dynamicLightColors");
    locations.dynamicLights.dynamicLightRadii=ArrayLoc(shader,"dynamicLightRadii");
    locations.dynamicLights.dynamicLightIntensities=ArrayLoc(shader,"dynamicLightIntensities");
    locations.dynamicLights.dynamicLightTypes=ArrayLoc(shader,"dynamicLightTypes");
    locations.dynamicLights.dynamicLightDirections=ArrayLoc(shader,"dynamicLightDirections");
    locations.dynamicLights.dynamicLightInnerConeCos=ArrayLoc(shader,"dynamicLightInnerConeCos");
    locations.dynamicLights.dynamicLightOuterConeCos=ArrayLoc(shader,"dynamicLightOuterConeCos");
    locations.shadows.dynamicLightShadowSlots=ArrayLoc(shader,"dynamicLightShadowSlots");
    for(std::size_t i=0;i<MaxDynamicSpotLightShadowCasters;++i) locations.shadows.shadowLightMatrices[i]=ArrayElementLoc(shader,"shadowLightMatrices",i);
    locations.shadows.shadowBias=ArrayLoc(shader,"shadowBias"); locations.shadows.shadowStrength=ArrayLoc(shader,"shadowStrength");
    locations.shadows.shadowSoftness=ArrayLoc(shader,"shadowSoftness"); locations.shadowMap0=GetShaderLocation(shader,"shadowMap0");
    locations.shadowMap1=GetShaderLocation(shader,"shadowMap1");
    compositeLocations.sceneColor=GetShaderLocation(compositeShader,"sceneColor");
    compositeLocations.sceneDepth=GetShaderLocation(compositeShader,"sceneDepth");
    compositeLocations.hazeTexture=GetShaderLocation(compositeShader,"hazeTexture");
    compositeLocations.hazeTexelSize=GetShaderLocation(compositeShader,"hazeTexelSize");
    compositeLocations.bilateralUpsample=GetShaderLocation(compositeShader,"bilateralUpsample");
    return true;
}

void SectorLightHazeRenderer::ReleaseTargets()
{
    engine::UnloadRenderTarget(hazeTarget);
    width=0; height=0; scale=0.0f;
}

bool SectorLightHazeRenderer::EnsureTargets(int newWidth, int newHeight, float newScale)
{
    if(engine::IsRenderTargetReady(hazeTarget)&&width==newWidth&&height==newHeight&&scale==newScale) return true;
    if(failedWidth==newWidth&&failedHeight==newHeight&&failedScale==newScale) return false;
    ReleaseTargets();
    const int targetWidth=std::max(1,static_cast<int>(std::round(newWidth*newScale)));
    const int targetHeight=std::max(1,static_cast<int>(std::round(newHeight*newScale)));
    std::string error;
    // RGB is premultiplied linear HDR in-scattered radiance; alpha is bounded opacity.
    engine::LoadRenderTarget(engine::RenderTargetDescriptor{"light-haze-accumulation",targetWidth,targetHeight,
            engine::RenderTargetColorFormat::Rgba16Float,engine::RenderTargetFilter::Bilinear,
            engine::RenderTargetWrap::Clamp,engine::RenderTargetDepthKind::None,1},hazeTarget,&error);
    if(!engine::IsRenderTargetReady(hazeTarget)) {
        failedWidth=newWidth; failedHeight=newHeight; failedScale=newScale;
        ReleaseTargets();
        accumulationDiagnostic="disabled: "+error;
        return false;
    }
    failedWidth=0; failedHeight=0; failedScale=0.0f;
    width=newWidth; height=newHeight; scale=newScale;
    accumulationDiagnostic=engine::FormatRenderTargetDiagnostic(hazeTarget);
    return true;
}

void SectorLightHazeRenderer::RefreshProbeIdentity(const SectorTopologyMap& map, const SectorBakedObjectLightProbeRuntimeData& probes)
{
    const auto* data=probes.probes.empty()?nullptr:probes.probes.data();
    const std::size_t hash=std::hash<std::string>{}(probes.metadata.sourceHash);
    const std::size_t mapHash=std::hash<std::string>{}(map.bakedLightmap.objectProbes.sourceHash);
    if(data==cachedProbeData&&probes.probes.size()==cachedProbeCount&&hash==cachedProbeHash&&mapHash==cachedMapProbeHash) return;
    for(auto& entry:probeCache) entry={}; cachedProbeData=data; cachedProbeCount=probes.probes.size(); cachedProbeHash=hash; cachedMapProbeHash=mapHash;
}

const SectorLightHazeStaticLightingSamples& SectorLightHazeRenderer::LightingFor(
        const SectorTopologyMap& map, const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorLightAtmosphereVolume& volume)
{
    ProbeCacheEntry* available=nullptr;
    for(auto& entry:probeCache) {
        if(entry.valid&&entry.kind==volume.source->kind&&entry.lightId==volume.source->lightId) {
            if(entry.ownerSectorId==volume.source->ownerSectorId&&SameVector(entry.origin,volume.originWorld)
                    &&SameVector(entry.direction,volume.directionWorld)&&entry.extent==volume.extentWorld&&entry.coneRadius==volume.coneRadiusWorld) return entry.lighting;
            available=&entry; break;
        }
        if(!entry.valid&&available==nullptr) available=&entry;
    }
    if(available==nullptr) available=&probeCache[static_cast<std::size_t>(std::max(volume.source->lightId,0))%probeCache.size()];
    available->valid=true; available->kind=volume.source->kind; available->lightId=volume.source->lightId;
    available->ownerSectorId=volume.source->ownerSectorId; available->origin=volume.originWorld;
    available->direction=volume.directionWorld; available->extent=volume.extentWorld; available->coneRadius=volume.coneRadiusWorld;
    available->lighting=SampleSectorLightHazeStaticLighting(map,probes,volume); return available->lighting;
}

bool SectorLightHazeRenderer::Apply(
        RenderTexture2D& sceneTarget, RenderTexture2D& sceneScratch,
        const SectorTopologyMap& map,
        SectorVolumetricQuality quality,
        const Camera3D& camera,
        float runtimeSeconds, const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds)
{
    eligibleCount=0; activeCount=0;
    if(sceneTarget.texture.id==0||sceneTarget.depth.id==0
            ||quality==SectorVolumetricQuality::Off) return false;
    const float nearPlane=static_cast<float>(rlGetCullDistanceNear()), farPlane=static_cast<float>(rlGetCullDistanceFar());
    if(!std::isfinite(nearPlane)||!std::isfinite(farPlane)||nearPlane<=0.0f||farPlane<=nearPlane) return false;
    const float aspect=static_cast<float>(sceneTarget.texture.width)/std::max(sceneTarget.texture.height,1);
    int cap=4, steps=8; float renderScale=0.5f;
    if(quality==SectorVolumetricQuality::Low) { cap=2; steps=4; renderScale=0.25f; }
    else if(quality==SectorVolumetricQuality::High) { cap=8; steps=12; renderScale=1.0f; }
    std::array<SectorLightAtmosphereVolume,MaxHazeVolumes> selected{}; std::array<float,MaxHazeVolumes> distances{};
    distances.fill(std::numeric_limits<float>::max());
    for(const auto& source:sources) {
        if(!source.atmosphere.haze.enabled||source.atmosphere.haze.density<=0.0f||!IsSectorLightAtmosphereSourceSelected(source,dynamicLights)) continue;
        SectorLightAtmosphereVolume volume; if(!MakeSectorLightAtmosphereVolume(source,source.atmosphere.haze.extentScale,source.atmosphere.haze.heightOffsetWorld,volume)) continue;
        if(!IsSectorLightAtmosphereVolumeVisible(volume,visibility,receiverBounds,camera,aspect,nearPlane,farPlane)) continue;
        ++eligibleCount; const float distance=Vector3DistanceSqr(camera.position,volume.boundsCenterWorld);
        int insert=std::min(activeCount,cap-1); if(activeCount>=cap&&distance>=distances[static_cast<std::size_t>(cap-1)]) continue;
        if(activeCount<cap) ++activeCount;
        while(insert>0&&distance<distances[static_cast<std::size_t>(insert-1)]) { selected[static_cast<std::size_t>(insert)]=selected[static_cast<std::size_t>(insert-1)]; distances[static_cast<std::size_t>(insert)]=distances[static_cast<std::size_t>(insert-1)]; --insert; }
        selected[static_cast<std::size_t>(insert)]=volume; distances[static_cast<std::size_t>(insert)]=distance;
    }
    if(activeCount==0) return false;
    if(!EnsureShaders()||!EnsureTargets(sceneTarget.texture.width,sceneTarget.texture.height,renderScale)) {
        if(!warnedUnavailable) { TraceLog(LOG_WARNING,"LIGHT HAZE: render resources unavailable; haze disabled"); warnedUnavailable=true; }
        return false;
    }
    RefreshProbeIdentity(map,probes);
    std::array<Vector3,MaxHazeVolumes> centers{},directions{},boundsCenters{},colors{};
    std::array<float,MaxHazeVolumes> boundsRadii{},extents{},coneRadii{}; std::array<int,MaxHazeVolumes> shapes{};
    std::array<int,MaxHazeVolumes> ownerDynamicLightIndices{}; ownerDynamicLightIndices.fill(-1);
    std::array<Vector4,MaxHazeVolumes> paramsA{},paramsB{}; std::array<Vector3,MaxHazeVolumes*8> lighting{};
    for(int i=0;i<activeCount;++i) {
        const auto& v=selected[static_cast<std::size_t>(i)]; const auto& h=v.source->atmosphere.haze;
        centers[i]=v.originWorld; directions[i]=v.directionWorld; boundsCenters[i]=v.boundsCenterWorld; boundsRadii[i]=v.boundsRadiusWorld;
        shapes[i]=v.source->shape==SectorLightAtmosphereShape::Cone?1:0; extents[i]=v.extentWorld; coneRadii[i]=v.coneRadiusWorld;
        colors[i]=engine::SrgbColorBytesToLinearSceneRgb(h.scatteringTint);
        paramsA[i]=Vector4{h.density,HazeMaximumOpacity,h.edgeSoftness,h.noiseScaleWorld};
        paramsB[i]=Vector4{h.noiseAmount,h.flowDirectionDegrees*DEG2RAD,h.flowSpeedWorld,h.heightOffsetWorld};
        ownerDynamicLightIndices[i]=FindOwnerDynamicLightIndex(*v.source,dynamicLights);
        const SectorLightAtmosphereVolume lightingVolume=UndisplacedLightingVolume(v,h.heightOffsetWorld);
        const auto& grid=LightingFor(map,probes,lightingVolume); for(int j=0;j<8;++j) lighting[static_cast<std::size_t>(i*8+j)]=grid.corners[static_cast<std::size_t>(j)];
    }
    const Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const Vector3 right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const Vector3 up=Vector3Normalize(Vector3CrossProduct(right,forward)); const float tanHalf=std::tan(camera.fovy*DEG2RAD*0.5f);
    const SectorFogRenderContext fog=BuildSectorFogRenderContext(map.fogSettings,camera.position);
    const SectorTopologyFogSettings& fogSettings=fog.settings;
    const int fogEnabled=fogSettings.enabled?1:0;
    const Vector3 fogColor=engine::SrgbColorBytesToLinearSceneRgb(fogSettings.color);
    rlDrawRenderBatchActive();
    BeginTextureMode(hazeTarget.native); ClearBackground(BLANK); BeginShaderMode(shader);
    SetShaderValueTexture(shader,locations.sceneDepth,sceneTarget.depth);
    if(locations.shadowMap0>=0&&dynamicLights.shadowMaps.shadowMap0!=nullptr&&dynamicLights.shadowMaps.shadowMap0->id!=0) SetShaderValueTexture(shader,locations.shadowMap0,*dynamicLights.shadowMaps.shadowMap0);
    if(locations.shadowMap1>=0&&dynamicLights.shadowMaps.shadowMap1!=nullptr&&dynamicLights.shadowMaps.shadowMap1->id!=0) SetShaderValueTexture(shader,locations.shadowMap1,*dynamicLights.shadowMaps.shadowMap1);
#define SET(field, value, type) SetShaderValue(shader,locations.field,&value,type)
    SET(volumeCount,activeCount,SHADER_UNIFORM_INT); SET(marchSteps,steps,SHADER_UNIFORM_INT); SET(cameraPosition,camera.position,SHADER_UNIFORM_VEC3);
    SET(cameraForward,forward,SHADER_UNIFORM_VEC3); SET(cameraRight,right,SHADER_UNIFORM_VEC3); SET(cameraUp,up,SHADER_UNIFORM_VEC3);
    SET(tanHalfFov,tanHalf,SHADER_UNIFORM_FLOAT); SET(aspectRatio,aspect,SHADER_UNIFORM_FLOAT); SET(nearPlane,nearPlane,SHADER_UNIFORM_FLOAT);
    SET(farPlane,farPlane,SHADER_UNIFORM_FLOAT); SET(runtimeSeconds,runtimeSeconds,SHADER_UNIFORM_FLOAT);
    SET(pathMinimumWorld,HazePathLimitSettings.minimumPathWorld,SHADER_UNIFORM_FLOAT);
    SET(pathThicknessMultiplier,HazePathLimitSettings.heightMultiplier,SHADER_UNIFORM_FLOAT);
    SET(pathSaturationPower,HazePathLimitSettings.saturationPower,SHADER_UNIFORM_FLOAT);
    SET(fogEnabled,fogEnabled,SHADER_UNIFORM_INT); SET(fogColor,fogColor,SHADER_UNIFORM_VEC3); SET(fogStartDistance,fogSettings.startDistanceWorld,SHADER_UNIFORM_FLOAT);
    SET(fogDensity,fogSettings.density,SHADER_UNIFORM_FLOAT); SET(fogMaxOpacity,fogSettings.maxOpacity,SHADER_UNIFORM_FLOAT);
    SET(fogReferenceHeight,fogSettings.referenceHeightWorld,SHADER_UNIFORM_FLOAT); SET(fogHeightFalloff,fogSettings.heightFalloff,SHADER_UNIFORM_FLOAT);
#undef SET
    SetShaderValueV(shader,locations.centers,centers.data(),SHADER_UNIFORM_VEC3,activeCount); SetShaderValueV(shader,locations.directions,directions.data(),SHADER_UNIFORM_VEC3,activeCount);
    SetShaderValueV(shader,locations.boundsCenters,boundsCenters.data(),SHADER_UNIFORM_VEC3,activeCount); SetShaderValueV(shader,locations.boundsRadii,boundsRadii.data(),SHADER_UNIFORM_FLOAT,activeCount);
    SetShaderValueV(shader,locations.shapes,shapes.data(),SHADER_UNIFORM_INT,activeCount); SetShaderValueV(shader,locations.extents,extents.data(),SHADER_UNIFORM_FLOAT,activeCount);
    SetShaderValueV(shader,locations.coneRadii,coneRadii.data(),SHADER_UNIFORM_FLOAT,activeCount); SetShaderValueV(shader,locations.colors,colors.data(),SHADER_UNIFORM_VEC3,activeCount);
    SetShaderValueV(shader,locations.paramsA,paramsA.data(),SHADER_UNIFORM_VEC4,activeCount); SetShaderValueV(shader,locations.paramsB,paramsB.data(),SHADER_UNIFORM_VEC4,activeCount);
    SetShaderValueV(shader,locations.ownerDynamicLightIndices,ownerDynamicLightIndices.data(),SHADER_UNIFORM_INT,activeCount);
    SetShaderValueV(shader,locations.staticLighting,lighting.data(),SHADER_UNIFORM_VEC3,activeCount*8);
    UploadSectorRendererDynamicPointLights(shader,locations.dynamicLights,dynamicLights); UploadSectorRendererDynamicSpotLightShadowUniforms(shader,locations.shadows,dynamicLights.shadowUniforms);
    rlDisableColorBlend(); DrawTexturePro(sceneTarget.texture,Src(sceneTarget.texture),Dst(hazeTarget.native.texture),{},0,WHITE);
    rlDrawRenderBatchActive(); EndShaderMode(); rlEnableColorBlend(); EndTextureMode();
    const Vector2 texel{1.0f/hazeTarget.native.texture.width,1.0f/hazeTarget.native.texture.height}; const int bilateral=quality==SectorVolumetricQuality::Medium?1:0;
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneScratch); ClearBackground(BLANK); BeginShaderMode(compositeShader);
    SetShaderValueTexture(compositeShader,compositeLocations.sceneColor,sceneTarget.texture); SetShaderValueTexture(compositeShader,compositeLocations.sceneDepth,sceneTarget.depth);
    SetShaderValueTexture(compositeShader,compositeLocations.hazeTexture,hazeTarget.native.texture); SetShaderValue(compositeShader,compositeLocations.hazeTexelSize,&texel,SHADER_UNIFORM_VEC2);
    SetShaderValue(compositeShader,compositeLocations.bilateralUpsample,&bilateral,SHADER_UNIFORM_INT); rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture,Src(sceneTarget.texture),Dst(sceneScratch.texture),{},0,WHITE);
    rlDrawRenderBatchActive(); EndShaderMode(); rlEnableColorBlend(); EndTextureMode(); return true;
}

void SectorLightHazeRenderer::Shutdown()
{
    ReleaseTargets(); if(Ready(shader)) UnloadShader(shader); if(Ready(compositeShader)) UnloadShader(compositeShader);
    shader={}; compositeShader={}; locations={}; compositeLocations={}; for(auto& entry:probeCache) entry={};
    cachedProbeData=nullptr; cachedProbeCount=0; cachedProbeHash=0; cachedMapProbeHash=0;
    failedWidth=0; failedHeight=0; failedScale=0.0f;
    shaderFailed=false;
    accumulationDiagnostic="not allocated";
}

} // namespace game
