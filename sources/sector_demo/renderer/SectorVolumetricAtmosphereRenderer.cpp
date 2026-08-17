#include "sector_demo/renderer/SectorVolumetricAtmosphereRenderer.h"

#include "engine/render/ColorTransfer.h"

#include <external/glad.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
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

const char* MediumInjectionFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D volumeData;
uniform sampler2D volumeLists;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform vec3 gridSize;
uniform int tileColumns;
uniform int sliceCount;
uniform int clusterBandCount;
uniform float sliceDepths[65];
uniform float runtimeSeconds;
uniform vec3 jitter;
uniform int fogEnabled;
uniform vec3 fogColor;
uniform float fogStartDistance;
uniform float fogDensity;
uniform float fogMaximumOpacity;
uniform float fogReferenceHeight;
uniform float fogHeightFalloff;
float hash31(vec3 p){p=fract(p*0.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
float valueNoise(vec3 p){vec3 c=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(mix(hash31(c),hash31(c+vec3(1,0,0)),f.x),mix(hash31(c+vec3(0,1,0)),hash31(c+vec3(1,1,0)),f.x),f.y),mix(mix(hash31(c+vec3(0,0,1)),hash31(c+vec3(1,0,1)),f.x),mix(hash31(c+vec3(0,1,1)),hash31(c+vec3(1,1,1)),f.x),f.y),f.z);}
float capDepth(float opacity){return -log(max(1.0-clamp(opacity,0.0,0.9999),0.0001));}
int listIndex(sampler2D lists,ivec2 cell,int band,int slot,ivec2 grid){int word=slot/4;int channel=slot-word*4;vec4 encodedIndices=texelFetch(lists,ivec2(cell.x*4+word,band*grid.y+cell.y),0);return int(floor(encodedIndices[channel]*255.0+0.5));}
void main(){
 ivec3 grid=ivec3(gridSize+vec3(0.5));ivec2 pixel=ivec2(gl_FragCoord.xy);ivec2 tile=pixel/ grid.xy;int z=tile.y*tileColumns+tile.x;
 if(z<0||z>=sliceCount){finalColor=vec4(0);return;}ivec2 cell=pixel-tile*grid.xy;int band=z/8;
 vec2 ndc=(vec2(cell)+vec2(0.5)+jitter.xy)/vec2(grid.xy)*2.0-1.0;vec3 ray=cameraForward+cameraRight*ndc.x*tanHalfFov*aspectRatio+cameraUp*ndc.y*tanHalfFov;
 float viewDepth=exp(mix(log(sliceDepths[z]),log(sliceDepths[z+1]),clamp(jitter.z,0.0,1.0)));vec3 p=cameraPosition+ray*viewDepth;float extinction=0.0;vec3 weightedTint=vec3(0);
 if(fogEnabled!=0&&viewDepth>=fogStartDistance&&fogDensity>0.0){float path=max(sliceDepths[sliceCount]-max(fogStartDistance,sliceDepths[0]),0.0001);float scale=min(1.0,capDepth(fogMaximumOpacity)/max(fogDensity*path,0.0001));float above=max(p.y-fogReferenceHeight,0.0);float sigma=fogDensity*exp(-above*fogHeightFalloff)*scale;extinction+=sigma;weightedTint+=fogColor*sigma;}
 for(int slot=0;slot<16;++slot){int index=listIndex(volumeLists,cell,band,slot,grid.xy);if(index==255)break;if(index<0||index>=254)continue;
  vec4 a=texelFetch(volumeData,ivec2(0,index),0),b=texelFetch(volumeData,ivec2(1,index),0),c=texelFetch(volumeData,ivec2(2,index),0),d=texelFetch(volumeData,ivec2(3,index),0),e=texelFetch(volumeData,ivec2(4,index),0);
  vec3 radii=max(b.xyz,vec3(0.0001));vec3 local=(p-a.xyz)/radii;float radius=length(local);if(radius>1.0)continue;float boundary=1.0-smoothstep(1.0-max(d.x,0.0001),1.0,radius);vec2 flow=vec2(cos(d.w),sin(d.w))*e.x*runtimeSeconds;vec3 np=p/max(d.z,0.05);np.xz+=flow;float noise=mix(1.0,mix(0.35,1.35,valueNoise(np)),clamp(d.y,0.0,1.0));float chord=2.0*max(radii.x,max(radii.y,radii.z));float scale=min(1.0,capDepth(c.a)/max(b.w*1.35*chord,0.0001));float sigma=max(b.w,0.0)*boundary*noise*scale;extinction+=sigma;weightedTint+=c.rgb*sigma;
 }
 finalColor=vec4(max(weightedTint,vec3(0)),max(extinction,0.0));
}
)";

const char* LightInjectionFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D mediumAtlas;
uniform sampler2D lightData;
uniform sampler2D lightLists;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform vec3 gridSize;
uniform int tileColumns;
uniform int sliceCount;
uniform int clusterBandCount;
uniform float sliceDepths[65];
uniform float runtimeSeconds;
uniform vec3 jitter;
uniform float anisotropy;
uniform mat4 shadowLightMatrices[2];
uniform float shadowBias[2];
uniform float shadowStrength[2];
uniform float shadowSoftness[2];
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
const float kPi=3.14159265358979323846;
const vec2 kPoissonDisk[12]=vec2[](vec2(-0.326,-0.406),vec2(-0.840,-0.074),vec2(-0.696,0.457),vec2(-0.203,0.621),vec2(0.962,-0.195),vec2(0.473,-0.480),vec2(0.519,0.767),vec2(0.185,-0.893),vec2(0.507,0.064),vec2(0.896,0.412),vec2(-0.322,-0.933),vec2(-0.792,-0.598));
vec3 safeNormalize(vec3 v,vec3 fallback){float l2=dot(v,v);return l2>0.00000001?v*inversesqrt(l2):fallback;}
float phaseHg(float cosineTheta){float g=clamp(anisotropy,-0.90,0.90);float d=max(1.0+g*g-2.0*g*clamp(cosineTheta,-1.0,1.0),0.000001);return (1.0-g*g)/(4.0*kPi*d*sqrt(d));}
int listIndex(ivec2 cell,int band,int slot,ivec2 grid){int word=slot/4;int channel=slot-word*4;vec4 encodedIndices=texelFetch(lightLists,ivec2(cell.x*4+word,band*grid.y+cell.y),0);return int(floor(encodedIndices[channel]*255.0+0.5));}
float sampleShadow(int slot,vec2 uv){return slot==0?texture(shadowMap0,uv).r:texture(shadowMap1,uv).r;}
float shadowVisibility(int slot,vec3 p){if(slot<0||slot>=2)return 1.0;vec4 clip=shadowLightMatrices[slot]*vec4(p,1.0);if(clip.w<=0.0)return 1.0;vec3 coord=clip.xyz/clip.w*0.5+0.5;if(any(lessThan(coord,vec3(0)))||any(greaterThan(coord,vec3(1))))return 1.0;float compareDepth=coord.z-min(max(shadowBias[slot],0.0),0.02);float softness=clamp(shadowSoftness[slot],0.0,8.0);if(softness<=0.0)return compareDepth<=sampleShadow(slot,coord.xy)?1.0:0.0;vec2 radius=max(0.25,softness)/vec2(textureSize(shadowMap0,0));float visible=0.0;for(int i=0;i<12;++i){float depth=sampleShadow(slot,clamp(coord.xy+kPoissonDisk[i]*radius,vec2(0),vec2(1)));visible+=compareDepth<=depth?1.0:0.0;}return visible/12.0;}
void main(){
 ivec3 grid=ivec3(gridSize+vec3(0.5));ivec2 pixel=ivec2(gl_FragCoord.xy);ivec2 tile=pixel/grid.xy;int z=tile.y*tileColumns+tile.x;if(z<0||z>=sliceCount){finalColor=vec4(0);return;}ivec2 cell=pixel-tile*grid.xy;vec4 medium=texelFetch(mediumAtlas,pixel,0);if(medium.a<=0.0){finalColor=vec4(0);return;}int band=z/8;
 vec2 ndc=(vec2(cell)+vec2(0.5)+jitter.xy)/vec2(grid.xy)*2.0-1.0;vec3 ray=cameraForward+cameraRight*ndc.x*tanHalfFov*aspectRatio+cameraUp*ndc.y*tanHalfFov;vec3 rayDirection=normalize(ray);float viewDepth=exp(mix(log(sliceDepths[z]),log(sliceDepths[z+1]),clamp(jitter.z,0.0,1.0)));vec3 p=cameraPosition+ray*viewDepth;vec3 direct=vec3(0);
 for(int slot=0;slot<16;++slot){int index=listIndex(cell,band,slot,grid.xy);if(index==255)break;if(index<0||index>=254)continue;vec4 a=texelFetch(lightData,ivec2(0,index),0),b=texelFetch(lightData,ivec2(1,index),0),c=texelFetch(lightData,ivec2(2,index),0),d=texelFetch(lightData,ivec2(3,index),0);vec3 toLight=a.xyz-p;float distance2=dot(toLight,toLight);float range=c.w;if(range<=0.0||distance2>=range*range)continue;float distanceToLight=sqrt(max(distance2,0.0));vec3 lightDirection=distanceToLight>0.0001?toLight/distanceToLight:vec3(0,1,0);float attenuation=clamp(1.0-distanceToLight/range,0.0,1.0);attenuation*=attenuation;float cone=1.0;if(a.w>0.5){vec3 spot=safeNormalize(c.xyz,vec3(0,-1,0));float coneDot=dot(spot,distanceToLight>0.0001?-lightDirection:spot);cone=abs(d.x-d.y)>0.0001?smoothstep(d.y,d.x,coneDot):step(d.x,coneDot);int shadowSlot=int(floor(d.w+0.5));if(shadowSlot>=0&&shadowSlot<2){float visibility=shadowVisibility(shadowSlot,p);cone*=mix(1.0,visibility,clamp(shadowStrength[shadowSlot],0.0,1.0));}}direct+=b.rgb*b.a*d.z*attenuation*cone*phaseHg(dot(lightDirection,-rayDirection));}
 finalColor=vec4(max(medium.rgb*direct,vec3(0)),medium.a);
}
)";

const char* IntegrateFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D lightingAtlas;
uniform sampler2D sceneDepth;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform vec3 gridSize;
uniform int tileColumns;
uniform int sliceCount;
uniform int clusterBandCount;
uniform float sliceDepths[65];
uniform float runtimeSeconds;
uniform vec3 jitter;
uniform float nearPlane;
uniform float farPlane;
float finiteHalf(float v){if(isnan(v))return 0.0;if(isinf(v))return v>0.0?65504.0:0.0;return clamp(v,0.0,65504.0);}
vec3 finiteRadiance(vec3 v){return vec3(finiteHalf(v.r),finiteHalf(v.g),finiteHalf(v.b));}
void main(){
 ivec3 grid=ivec3(gridSize+vec3(0.5));ivec2 cell=ivec2(gl_FragCoord.xy);vec2 sampleUv=clamp(fragUv+jitter.xy/vec2(grid.xy),vec2(0),vec2(1));vec2 ndc=sampleUv*2.0-1.0;vec3 ray=cameraForward+cameraRight*ndc.x*tanHalfFov*aspectRatio+cameraUp*ndc.y*tanHalfFov;vec3 rayDirection=normalize(ray);float cosineForward=max(dot(rayDirection,cameraForward),0.0001);float depth=texture(sceneDepth,sampleUv).r;float sceneForward=sliceDepths[sliceCount];if(!isnan(depth)&&!isinf(depth)&&depth<0.999999){float zNdc=clamp(depth,0.0,1.0)*2.0-1.0;sceneForward=min(sceneForward,(2.0*nearPlane*farPlane)/max(farPlane+nearPlane-zNdc*(farPlane-nearPlane),0.00001));}
 float transmittance=1.0;vec3 radiance=vec3(0);for(int z=0;z<64;++z){if(z>=sliceCount||sliceDepths[z]>=sceneForward||transmittance<=0.0001)break;float segmentEnd=min(sliceDepths[z+1],sceneForward);float lengthWorld=max(segmentEnd-sliceDepths[z],0.0)/cosineForward;if(lengthWorld<=0.0)continue;ivec2 atlas=ivec2((z%tileColumns)*grid.x+cell.x,(z/tileColumns)*grid.y+cell.y);vec4 lighting=texelFetch(lightingAtlas,atlas,0);float sigma=max(lighting.a,0.0);if(sigma<=0.0)continue;float stepTransmittance=exp(-sigma*lengthWorld);float stepOpacity=1.0-stepTransmittance;vec3 source=max(lighting.rgb,vec3(0))/max(sigma,0.000001);radiance+=transmittance*stepOpacity*source;transmittance*=stepTransmittance;}
 float storedNdc=(farPlane+nearPlane-(2.0*nearPlane*farPlane)/max(sceneForward,nearPlane))/(farPlane-nearPlane);gl_FragDepth=clamp(storedNdc*0.5+0.5,0.0,1.0);finalColor=vec4(finiteRadiance(radiance),clamp(1.0-transmittance,0.0,1.0));
}
)";

const char* TemporalResolveFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D currentAtmosphere;
uniform sampler2D currentDepth;
uniform sampler2D historyAtmosphere;
uniform sampler2D historyDepth;
uniform mat4 inverseCurrentViewProjection;
uniform mat4 previousViewProjection;
uniform vec2 currentJitterUv;
uniform vec2 previousJitterUv;
uniform vec2 texelSize;
uniform float nearPlane;
uniform float farPlane;
uniform int historyValid;
uniform float baseCurrentWeight;
uniform float responsiveCurrentWeight;
uniform int outputHistoryWeight;
float linearDepth(float depth){float z=clamp(depth,0.0,1.0)*2.0-1.0;return (2.0*nearPlane*farPlane)/max(farPlane+nearPlane-z*(farPlane-nearPlane),0.00001);}
float finiteHalf(float v){if(isnan(v))return 0.0;if(isinf(v))return v>0.0?65504.0:0.0;return clamp(v,0.0,65504.0);}
vec4 sanitizeAtmosphere(vec4 v){return vec4(finiteHalf(v.r),finiteHalf(v.g),finiteHalf(v.b),(isnan(v.a)||isinf(v.a))?0.0:clamp(v.a,0.0,1.0));}
void main(){ivec2 size=textureSize(currentAtmosphere,0);ivec2 cell=clamp(ivec2(gl_FragCoord.xy),ivec2(0),size-ivec2(1));vec4 current=sanitizeAtmosphere(texelFetch(currentAtmosphere,cell,0));float depth=texelFetch(currentDepth,cell,0).r;gl_FragDepth=depth;vec4 minimumValue=vec4(65504.0);vec4 maximumValue=vec4(0);for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){vec4 value=sanitizeAtmosphere(texelFetch(currentAtmosphere,clamp(cell+ivec2(x,y),ivec2(0),size-ivec2(1)),0));minimumValue=min(minimumValue,value);maximumValue=max(maximumValue,value);}float historyWeight=0.0;vec4 resolved=current;if(historyValid!=0&&!isnan(depth)&&!isinf(depth)){vec2 sampledUv=fragUv+currentJitterUv;vec4 clip=vec4(sampledUv*2.0-1.0,depth*2.0-1.0,1.0);vec4 world=inverseCurrentViewProjection*clip;if(abs(world.w)>0.000001){world/=world.w;vec4 previousClip=previousViewProjection*world;if(previousClip.w>0.000001){vec3 previousNdc=previousClip.xyz/previousClip.w;vec2 historyUv=previousNdc.xy*0.5+0.5-previousJitterUv;float expectedDepth=previousNdc.z*0.5+0.5;if(all(greaterThanEqual(historyUv,vec2(0)))&&all(lessThanEqual(historyUv,vec2(1)))&&expectedDepth>=0.0&&expectedDepth<=1.0){float previousDepth=texture(historyDepth,historyUv).r;float expectedWorld=linearDepth(expectedDepth);float sampledWorld=linearDepth(previousDepth);float tolerance=max(0.10,expectedWorld*0.02);if(abs(expectedWorld-sampledWorld)<=tolerance){vec4 history=sanitizeAtmosphere(texture(historyAtmosphere,historyUv));history=clamp(history,minimumValue,maximumValue);float currentLuma=dot(current.rgb,vec3(0.2126,0.7152,0.0722));float delta=max(length(history.rgb-current.rgb)/max(currentLuma+0.05,0.05),abs(history.a-current.a)*2.0);float currentWeight=mix(baseCurrentWeight,responsiveCurrentWeight,clamp(delta,0.0,1.0));historyWeight=1.0-currentWeight;resolved=mix(history,current,currentWeight);}}}}}if(outputHistoryWeight!=0){finalColor=vec4(vec3(historyWeight),1.0);return;}finalColor=sanitizeAtmosphere(resolved);}
)";

const char* CompositeFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D atmosphereTexture;
uniform sampler2D atmosphereDepth;
uniform sampler2D mediumAtlas;
uniform sampler2D lightingAtlas;
uniform vec2 atmosphereTexelSize;
uniform float nearPlane;
uniform float farPlane;
uniform float maximumDistance;
uniform int debugView;
uniform int historyAvailable;
float finiteHalf(float v){if(isnan(v))return 0.0;if(isinf(v))return v>0.0?65504.0:0.0;return clamp(v,0.0,65504.0);}
vec3 finiteRadiance(vec3 v){return vec3(finiteHalf(v.r),finiteHalf(v.g),finiteHalf(v.b));}
float linearDepth(float depth){if(isnan(depth)||isinf(depth)||depth>=0.999999)return maximumDistance;float z=clamp(depth,0.0,1.0)*2.0-1.0;return min(maximumDistance,(2.0*nearPlane*farPlane)/max(farPlane+nearPlane-z*(farPlane-nearPlane),0.00001));}
void main(){vec4 scene=texture(sceneColor,fragUv);float alpha=(isnan(scene.a)||isinf(scene.a))?1.0:clamp(scene.a,0.0,1.0);if(debugView==1){vec4 medium=texture(mediumAtlas,fragUv);vec4 lighting=texture(lightingAtlas,fragUv);finalColor=vec4(finiteRadiance(lighting.rgb+medium.rgb*0.25+vec3(medium.a*0.05)),alpha);return;}if(debugView==2){vec3 weight=historyAvailable!=0?texture(atmosphereTexture,fragUv).rgb:vec3(0);finalColor=vec4(clamp(weight,vec3(0),vec3(1)),alpha);return;}ivec2 size=textureSize(atmosphereTexture,0);vec2 position=fragUv*vec2(size)-vec2(0.5);ivec2 base=ivec2(floor(position));vec2 fraction=fract(position);float centerWorld=linearDepth(texture(sceneDepth,fragUv).r);vec4 atmosphere=vec4(0);float totalWeight=0.0;float closestDifference=1e30;vec4 closest=vec4(0);for(int y=0;y<=1;++y)for(int x=0;x<=1;++x){ivec2 cell=clamp(base+ivec2(x,y),ivec2(0),size-ivec2(1));vec4 sampleValue=texelFetch(atmosphereTexture,cell,0);float sampleWorld=linearDepth(texelFetch(atmosphereDepth,cell,0).r);float difference=abs(sampleWorld-centerWorld);float spatial=(x==0?1.0-fraction.x:fraction.x)*(y==0?1.0-fraction.y:fraction.y);float depthWeight=exp(-difference/max(0.05,centerWorld*0.02));float weight=spatial*depthWeight;atmosphere+=sampleValue*weight;totalWeight+=weight;if(difference<closestDifference){closestDifference=difference;closest=sampleValue;}}atmosphere=totalWeight>0.0001?atmosphere/totalWeight:closest;float opacity=(isnan(atmosphere.a)||isinf(atmosphere.a))?0.0:clamp(atmosphere.a,0.0,1.0);vec3 composed=finiteRadiance(scene.rgb)*(1.0-opacity)+finiteRadiance(atmosphere.rgb);finalColor=vec4(finiteRadiance(composed),alpha);}
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

void ClearGlErrors()
{
    while (glGetError() != GL_NO_ERROR) {
    }
}

bool UploadTexture(Texture2D texture, GLenum type, const void* data)
{
    if (texture.id == 0 || data == nullptr) return false;
    int previousBinding = 0;
    int previousUnpack = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBinding);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpack);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ClearGlErrors();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            texture.width, texture.height, GL_RGBA, type, data);
    const bool valid = glGetError() == GL_NO_ERROR;
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpack);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBinding));
    return valid;
}

void HashBytes(std::uint64_t& hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
}

std::uint64_t FogSignature(const SectorTopologyFogSettings& settings)
{
    std::uint64_t hash = 1469598103934665603ull;
    HashBytes(hash, &settings.enabled, sizeof(settings.enabled));
    HashBytes(hash, &settings.color, sizeof(settings.color));
    HashBytes(hash, &settings.startDistanceWorld, sizeof(settings.startDistanceWorld));
    HashBytes(hash, &settings.density, sizeof(settings.density));
    HashBytes(hash, &settings.maxOpacity, sizeof(settings.maxOpacity));
    HashBytes(hash, &settings.referenceHeightWorld, sizeof(settings.referenceHeightWorld));
    HashBytes(hash, &settings.heightFalloff, sizeof(settings.heightFalloff));
    HashBytes(hash, &settings.anisotropy, sizeof(settings.anisotropy));
    HashBytes(hash, &settings.volumetricMaxDistanceWorld,
            sizeof(settings.volumetricMaxDistanceWorld));
    return hash;
}

std::uint64_t AtmosphereSourceSignature(
        std::uint64_t sourceRevision,
        const SectorTopologyMap& map,
        const std::vector<SectorLightAtmosphereSource>& lightSources)
{
    std::uint64_t hash = 1469598103934665603ull;
    HashBytes(hash, &sourceRevision, sizeof(sourceRevision));
    const std::size_t volumeCount = map.compiledLocalFogVolumes.size();
    HashBytes(hash, &volumeCount, sizeof(volumeCount));
    for (const SectorCompiledLocalFogVolume& volume : map.compiledLocalFogVolumes) {
#define HASH_VOLUME(field) HashBytes(hash, &volume.field, sizeof(volume.field))
        HASH_VOLUME(sourceAuthoringFogVolumeId);
        HASH_VOLUME(topologySectorId);
        HASH_VOLUME(enabled);
        HASH_VOLUME(centerWorld);
        HASH_VOLUME(radiiWorld);
        HASH_VOLUME(color);
        HASH_VOLUME(density);
        HASH_VOLUME(maxOpacity);
        HASH_VOLUME(edgeSoftness);
        HASH_VOLUME(noiseScaleWorld);
        HASH_VOLUME(noiseAmount);
        HASH_VOLUME(flowDirectionDegrees);
        HASH_VOLUME(flowSpeedWorld);
#undef HASH_VOLUME
    }
    const std::size_t lightCount = lightSources.size();
    HashBytes(hash, &lightCount, sizeof(lightCount));
    for (const SectorLightAtmosphereSource& light : lightSources) {
#define HASH_LIGHT(field) HashBytes(hash, &light.field, sizeof(light.field))
        HASH_LIGHT(kind);
        HASH_LIGHT(shape);
        HASH_LIGHT(lightId);
        HASH_LIGHT(ownerSectorId);
        HASH_LIGHT(positionWorld);
        HASH_LIGHT(directionWorld);
        HASH_LIGHT(color);
        HASH_LIGHT(intensity);
        HASH_LIGHT(rangeWorld);
        HASH_LIGHT(innerConeCos);
        HASH_LIGHT(outerConeCos);
        HASH_LIGHT(flicker);
        HASH_LIGHT(flickerSpeed);
        HASH_LIGHT(flickerAmount);
        HASH_LIGHT(atmosphere.volumetricScatteringIntensity);
#undef HASH_LIGHT
    }
    return hash;
}

Matrix CameraViewProjection(
        const Camera3D& camera,
        float aspect,
        float nearPlane,
        float farPlane)
{
    const Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    const Matrix projection = MatrixPerspective(
            static_cast<double>(camera.fovy * DEG2RAD),
            static_cast<double>(aspect),
            static_cast<double>(nearPlane),
            static_cast<double>(farPlane));
    return MatrixMultiply(view, projection);
}

bool ValidShadowTexture(const Texture2D* texture)
{
    return texture != nullptr && texture->id != 0
            && texture->width > 0 && texture->height > 0;
}

bool ValidShadowSlot(
        int slot,
        const SectorBillboardDynamicLightContext& context)
{
    if (slot == 0) return ValidShadowTexture(context.shadowMaps.shadowMap0);
    if (slot == 1) return ValidShadowTexture(context.shadowMaps.shadowMap1);
    return false;
}

void BeginAlwaysDepthWrite()
{
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);
}

void EndAlwaysDepthWrite()
{
    rlDrawRenderBatchActive();
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    rlDisableDepthTest();
}

} // namespace

void SectorVolumetricAtmosphereRenderer::CaptureCommonLocations(
        Shader value,
        CommonLocations& locations)
{
#define LOC(field, name) locations.field = GetShaderLocation(value, name)
    LOC(cameraPosition, "cameraPosition");
    LOC(cameraForward, "cameraForward");
    LOC(cameraRight, "cameraRight");
    LOC(cameraUp, "cameraUp");
    LOC(tanHalfFov, "tanHalfFov");
    LOC(aspectRatio, "aspectRatio");
    LOC(gridSize, "gridSize");
    LOC(tileColumns, "tileColumns");
    LOC(sliceCount, "sliceCount");
    LOC(clusterBandCount, "clusterBandCount");
    locations.sliceDepths = ArrayLocation(value, "sliceDepths");
    LOC(runtimeSeconds, "runtimeSeconds");
    LOC(jitter, "jitter");
#undef LOC
}

bool SectorVolumetricAtmosphereRenderer::Initialize()
{
    if (mediumShader.id != 0 && lightShader.id != 0
            && integrationShader.id != 0 && temporalShader.id != 0
            && compositeShader.id != 0) {
        return true;
    }
    if (shaderFailed) return false;
    mediumShader = LoadShaderFromMemory(FullscreenVs, MediumInjectionFs);
    lightShader = LoadShaderFromMemory(FullscreenVs, LightInjectionFs);
    integrationShader = LoadShaderFromMemory(FullscreenVs, IntegrateFs);
    temporalShader = LoadShaderFromMemory(FullscreenVs, TemporalResolveFs);
    compositeShader = LoadShaderFromMemory(FullscreenVs, CompositeFs);
    if (mediumShader.id == 0 || lightShader.id == 0
            || integrationShader.id == 0 || temporalShader.id == 0
            || compositeShader.id == 0) {
        if (mediumShader.id != 0) UnloadShader(mediumShader);
        if (lightShader.id != 0) UnloadShader(lightShader);
        if (integrationShader.id != 0) UnloadShader(integrationShader);
        if (temporalShader.id != 0) UnloadShader(temporalShader);
        if (compositeShader.id != 0) UnloadShader(compositeShader);
        mediumShader = {};
        lightShader = {};
        integrationShader = {};
        temporalShader = {};
        compositeShader = {};
        shaderFailed = true;
        resourceDiagnostic = "disabled: unified froxel shader unavailable";
        return false;
    }
    CaptureCommonLocations(mediumShader, mediumLocations);
    CaptureCommonLocations(lightShader, lightLocations);
    CaptureCommonLocations(integrationShader, integrationLocations);
#define MLOC(field, name) mediumLocations.field = GetShaderLocation(mediumShader, name)
    MLOC(volumeData, "volumeData");
    MLOC(volumeLists, "volumeLists");
    MLOC(fogEnabled, "fogEnabled");
    MLOC(fogColor, "fogColor");
    MLOC(fogStartDistance, "fogStartDistance");
    MLOC(fogDensity, "fogDensity");
    MLOC(fogMaximumOpacity, "fogMaximumOpacity");
    MLOC(fogReferenceHeight, "fogReferenceHeight");
    MLOC(fogHeightFalloff, "fogHeightFalloff");
#undef MLOC
#define LLOC(field, name) lightLocations.field = GetShaderLocation(lightShader, name)
    LLOC(mediumAtlas, "mediumAtlas");
    LLOC(lightData, "lightData");
    LLOC(lightLists, "lightLists");
    LLOC(anisotropy, "anisotropy");
    for (std::size_t index = 0; index < MaxDynamicSpotLightShadowCasters; ++index) {
        lightLocations.shadowLightMatrices[index] = GetShaderLocation(
                lightShader,
                (std::string("shadowLightMatrices[")
                        + std::to_string(index) + "]").c_str());
    }
    lightLocations.shadowBias = ArrayLocation(lightShader, "shadowBias");
    lightLocations.shadowStrength = ArrayLocation(lightShader, "shadowStrength");
    lightLocations.shadowSoftness = ArrayLocation(lightShader, "shadowSoftness");
    LLOC(shadowMap0, "shadowMap0");
    LLOC(shadowMap1, "shadowMap1");
#undef LLOC
#define ILOC(field, name) integrationLocations.field = GetShaderLocation(integrationShader, name)
    ILOC(lightingAtlas, "lightingAtlas");
    ILOC(sceneDepth, "sceneDepth");
    ILOC(nearPlane, "nearPlane");
    ILOC(farPlane, "farPlane");
#undef ILOC
#define TLOC(field, name) temporalLocations.field = GetShaderLocation(temporalShader, name)
    TLOC(currentAtmosphere, "currentAtmosphere");
    TLOC(currentDepth, "currentDepth");
    TLOC(historyAtmosphere, "historyAtmosphere");
    TLOC(historyDepth, "historyDepth");
    TLOC(inverseCurrentViewProjection, "inverseCurrentViewProjection");
    TLOC(previousViewProjection, "previousViewProjection");
    TLOC(currentJitterUv, "currentJitterUv");
    TLOC(previousJitterUv, "previousJitterUv");
    TLOC(texelSize, "texelSize");
    TLOC(nearPlane, "nearPlane");
    TLOC(farPlane, "farPlane");
    TLOC(historyValid, "historyValid");
    TLOC(baseCurrentWeight, "baseCurrentWeight");
    TLOC(responsiveCurrentWeight, "responsiveCurrentWeight");
    TLOC(outputHistoryWeight, "outputHistoryWeight");
#undef TLOC
    compositeLocations.sceneColor = GetShaderLocation(compositeShader, "sceneColor");
    compositeLocations.sceneDepth = GetShaderLocation(compositeShader, "sceneDepth");
    compositeLocations.atmosphereTexture = GetShaderLocation(compositeShader, "atmosphereTexture");
    compositeLocations.atmosphereTexelSize = GetShaderLocation(compositeShader, "atmosphereTexelSize");
    compositeLocations.atmosphereDepth = GetShaderLocation(compositeShader, "atmosphereDepth");
    compositeLocations.mediumAtlas = GetShaderLocation(compositeShader, "mediumAtlas");
    compositeLocations.lightingAtlas = GetShaderLocation(compositeShader, "lightingAtlas");
    compositeLocations.nearPlane = GetShaderLocation(compositeShader, "nearPlane");
    compositeLocations.farPlane = GetShaderLocation(compositeShader, "farPlane");
    compositeLocations.maximumDistance = GetShaderLocation(compositeShader, "maximumDistance");
    compositeLocations.debugView = GetShaderLocation(compositeShader, "debugView");
    compositeLocations.historyAvailable = GetShaderLocation(compositeShader, "historyAvailable");
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    resourceDiagnostic = "froxel shaders ready; resources not allocated";
    return maximumTextureSize > 0;
}

bool SectorVolumetricAtmosphereRenderer::AllocateDataTexture(
        int width,
        int height,
        int pixelFormat,
        Texture2D& texture,
        const char* debugName)
{
    texture = {};
    texture.id = rlLoadTexture(nullptr, width, height, pixelFormat, 1);
    texture.width = width;
    texture.height = height;
    texture.mipmaps = 1;
    texture.format = pixelFormat;
    if (texture.id == 0) {
        resourceDiagnostic = std::string("disabled: could not allocate ") + debugName;
        return false;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    return true;
}

bool SectorVolumetricAtmosphereRenderer::EnsureResources(
        const SectorVolumetricResourceLayout& layout)
{
    if (layout.quality == SectorTopologyFogSettings::VolumetricQuality::Off) {
        resourceDiagnostic = "disabled: froxel atlases exceed GL texture limit";
        return false;
    }
    if (engine::IsRenderTargetReady(mediumAtlas)
            && resourceLayout.quality == layout.quality
            && resourceLayout.grid.x == layout.grid.x
            && resourceLayout.grid.y == layout.grid.y
            && resourceLayout.grid.z == layout.grid.z) {
        return true;
    }
    ReleaseResources();
    resourceLayout = layout;
    const auto loadTarget = [](const char* name, int width, int height,
                               engine::RenderTargetFilter filter,
                               engine::RenderTargetDepthKind depth,
                               engine::RenderTarget& target,
                               std::string& error) {
        return engine::LoadRenderTarget(
                engine::RenderTargetDescriptor{
                        name, width, height,
                        engine::RenderTargetColorFormat::Rgba16Float,
                        filter, engine::RenderTargetWrap::Clamp,
                        depth, 1},
                target, &error);
    };
    std::string error;
    if (!loadTarget("volumetric-medium-atlas", layout.atlas.width,
                    layout.atlas.height, engine::RenderTargetFilter::Point,
                    engine::RenderTargetDepthKind::None,
                    mediumAtlas, error)
            || !loadTarget("volumetric-lighting-atlas", layout.atlas.width,
                    layout.atlas.height, engine::RenderTargetFilter::Point,
                    engine::RenderTargetDepthKind::None,
                    lightingAtlas, error)
            || !loadTarget("volumetric-integrated", layout.integratedWidth,
                    layout.integratedHeight, engine::RenderTargetFilter::Bilinear,
                    engine::RenderTargetDepthKind::SampleableTexture,
                    integratedTarget, error)
            || (GetSectorVolumetricTemporalPolicy(layout.quality).enabled
                    && (!loadTarget("volumetric-history-a", layout.integratedWidth,
                            layout.integratedHeight,
                            engine::RenderTargetFilter::Bilinear,
                            engine::RenderTargetDepthKind::SampleableTexture,
                            historyTargets[0], error)
                        || !loadTarget("volumetric-history-b", layout.integratedWidth,
                            layout.integratedHeight,
                            engine::RenderTargetFilter::Bilinear,
                            engine::RenderTargetDepthKind::SampleableTexture,
                            historyTargets[1], error)))
            || !AllocateDataTexture(layout.lightDataWidth,
                    layout.lightDataHeight,
                    PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
                    lightDataTexture, "volumetric light data texture")
            || !AllocateDataTexture(layout.volumeDataWidth,
                    layout.volumeDataHeight,
                    PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
                    volumeDataTexture, "volumetric volume data texture")
            || !AllocateDataTexture(layout.clusters.width,
                    layout.clusters.height,
                    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                    lightListTexture, "volumetric light-list texture")
            || !AllocateDataTexture(layout.clusters.width,
                    layout.clusters.height,
                    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                    volumeListTexture, "volumetric volume-list texture")
            || !clusterBuilder.Configure(
                    layout.grid,
                    GetSectorVolumetricQualityContract(
                            layout.quality).clusterBands)) {
        if (!error.empty()) resourceDiagnostic = "disabled: " + error;
        ReleaseResources();
        resourceLayout = layout;
        return false;
    }
    estimatedResourceBytes = mediumAtlas.actual.estimatedAllocationBytes
            + lightingAtlas.actual.estimatedAllocationBytes
            + integratedTarget.actual.estimatedAllocationBytes
            + historyTargets[0].actual.estimatedAllocationBytes
            + historyTargets[1].actual.estimatedAllocationBytes
            + EstimateSectorAtmosphereTargetBytes(
                    layout.lightDataWidth, layout.lightDataHeight, 16)
            + EstimateSectorAtmosphereTargetBytes(
                    layout.volumeDataWidth, layout.volumeDataHeight, 16)
            + layout.clusters.estimatedBytes * 2u;
    std::ostringstream diagnostic;
    diagnostic << "ready: grid " << layout.grid.x << 'x' << layout.grid.y
               << 'x' << layout.grid.z << " atlas " << layout.atlas.width
               << 'x' << layout.atlas.height;
    resourceDiagnostic = diagnostic.str();
    if (integratedTarget.native.depth.id != 0) {
        SetTextureFilter(integratedTarget.native.depth, TEXTURE_FILTER_POINT);
        SetTextureWrap(integratedTarget.native.depth, TEXTURE_WRAP_CLAMP);
    }
    for (engine::RenderTarget& target : historyTargets) {
        if (target.native.depth.id != 0) {
            SetTextureFilter(target.native.depth, TEXTURE_FILTER_POINT);
            SetTextureWrap(target.native.depth, TEXTURE_WRAP_CLAMP);
        }
    }
    InvalidateHistory(SectorVolumetricHistoryResetReason::AtlasLayoutChanged);
    warnedUnavailable = false;
    return true;
}

bool SectorVolumetricAtmosphereRenderer::BuildAndUploadDataTextures()
{
    lightDataStaging.fill(0.0f);
    volumeDataStaging.fill(0.0f);
    const auto& lights = clusterBuilder.Lights();
    shadowedSpotLightCount = 0;
    for (int index = 0; index < clusterBuilder.Diagnostics().retainedLightCount; ++index) {
        const auto& light = lights[static_cast<std::size_t>(index)];
        const std::size_t base = static_cast<std::size_t>(index)
                * SectorVolumetricLightRecordTexels * 4;
        const bool spot = light.kind == SectorLightAtmosphereSourceKind::StaticSpot
                || light.kind == SectorLightAtmosphereSourceKind::DynamicSpot;
        lightDataStaging[base + 0] = light.positionWorld.x;
        lightDataStaging[base + 1] = light.positionWorld.y;
        lightDataStaging[base + 2] = light.positionWorld.z;
        lightDataStaging[base + 3] = spot ? 1.0f : 0.0f;
        lightDataStaging[base + 4] = light.linearColor.x;
        lightDataStaging[base + 5] = light.linearColor.y;
        lightDataStaging[base + 6] = light.linearColor.z;
        lightDataStaging[base + 7] = light.effectiveIntensity;
        lightDataStaging[base + 8] = light.directionWorld.x;
        lightDataStaging[base + 9] = light.directionWorld.y;
        lightDataStaging[base + 10] = light.directionWorld.z;
        lightDataStaging[base + 11] = light.rangeWorld;
        lightDataStaging[base + 12] = light.innerConeCos;
        lightDataStaging[base + 13] = light.outerConeCos;
        lightDataStaging[base + 14] = light.scatteringIntensity;
        const int shadowSlot = ValidShadowSlot(
                light.shadowSlot, preparedDynamicLights)
                ? light.shadowSlot : -1;
        lightDataStaging[base + 15] = static_cast<float>(shadowSlot);
        if (shadowSlot >= 0) ++shadowedSpotLightCount;
    }
    const auto& volumes = clusterBuilder.Volumes();
    for (int index = 0; index < clusterBuilder.Diagnostics().retainedVolumeCount; ++index) {
        const auto& volume = volumes[static_cast<std::size_t>(index)];
        const std::size_t base = static_cast<std::size_t>(index)
                * SectorVolumetricVolumeRecordTexels * 4;
        volumeDataStaging[base + 0] = volume.centerWorld.x;
        volumeDataStaging[base + 1] = volume.centerWorld.y;
        volumeDataStaging[base + 2] = volume.centerWorld.z;
        volumeDataStaging[base + 4] = volume.radiiWorld.x;
        volumeDataStaging[base + 5] = volume.radiiWorld.y;
        volumeDataStaging[base + 6] = volume.radiiWorld.z;
        volumeDataStaging[base + 7] = volume.density;
        volumeDataStaging[base + 8] = volume.linearTint.x;
        volumeDataStaging[base + 9] = volume.linearTint.y;
        volumeDataStaging[base + 10] = volume.linearTint.z;
        volumeDataStaging[base + 11] = volume.maximumOpacity;
        volumeDataStaging[base + 12] = volume.edgeSoftness;
        volumeDataStaging[base + 13] = volume.noiseAmount;
        volumeDataStaging[base + 14] = volume.noiseScaleWorld;
        volumeDataStaging[base + 15] = volume.flowDirectionRadians;
        volumeDataStaging[base + 16] = volume.flowSpeedWorld;
    }
    return UploadTexture(lightDataTexture, GL_FLOAT, lightDataStaging.data())
            && UploadTexture(volumeDataTexture, GL_FLOAT, volumeDataStaging.data())
            && UploadTexture(lightListTexture, GL_UNSIGNED_BYTE,
                    clusterBuilder.LightClusterIndices().data())
            && UploadTexture(volumeListTexture, GL_UNSIGNED_BYTE,
                    clusterBuilder.VolumeClusterIndices().data());
}

bool SectorVolumetricAtmosphereRenderer::Prepare(
        const engine::RenderTarget& sceneTarget,
        const SectorTopologyMap& map,
        SectorTopologyFogSettings::VolumetricQuality quality,
        const Camera3D& camera,
        float runtimeSeconds,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const SectorPreviewDynamicPointLightSource* runtimePointLight,
        const std::vector<SectorLightAtmosphereSource>& lightAtmosphereSources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        bool dynamicLightingEnabled,
        std::uint64_t sourceRevision)
{
    ResetPreparedFrame();
    prepared = true;
    requestedQuality = quality;
    fogSettings = NormalizeSectorTopologyFogSettings(map.fogSettings);
    globalFogActive = fogSettings.enabled && fogSettings.density > 0.0f
            && fogSettings.maxOpacity > 0.0f;
    preparedCamera = camera;
    preparedDynamicLights = dynamicLights;
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
        InvalidateHistory(quality == SectorTopologyFogSettings::VolumetricQuality::Off
                ? SectorVolumetricHistoryResetReason::InactiveFrame
                : SectorVolumetricHistoryResetReason::ResourceUnavailable);
        return false;
    }
    if (!Initialize()) {
        InvalidateHistory(SectorVolumetricHistoryResetReason::ResourceUnavailable);
        return false;
    }
    const int sceneWidth = sceneTarget.native.texture.width;
    const int sceneHeight = sceneTarget.native.texture.height;
    const SectorVolumetricResourceLayout resolved =
            ResolveSectorVolumetricResourceLayout(
                    quality, sceneWidth, sceneHeight, maximumTextureSize);
    if (failedSceneWidth == sceneWidth && failedSceneHeight == sceneHeight
            && failedQuality == resolved.quality) {
        return false;
    }
    if (!EnsureResources(resolved)) {
        failedSceneWidth = sceneWidth;
        failedSceneHeight = sceneHeight;
        failedQuality = resolved.quality;
        if (!warnedUnavailable) {
            TraceLog(LOG_WARNING,
                    "UNIFIED ATMOSPHERE: froxel resources unavailable; using analytic fog fallback");
            warnedUnavailable = true;
        }
        InvalidateHistory(SectorVolumetricHistoryResetReason::ResourceUnavailable);
        return false;
    }
    failedSceneWidth = 0;
    failedSceneHeight = 0;
    failedQuality = SectorTopologyFogSettings::VolumetricQuality::Off;
    aspectRatio = static_cast<float>(sceneWidth) / std::max(sceneHeight, 1);
    temporalPolicy = GetSectorVolumetricTemporalPolicy(resolved.quality);
    currentJitter = ComputeSectorVolumetricJitter(
            resolved.quality, temporalFrameIndex);
    currentViewProjection = CameraViewProjection(
            camera, aspectRatio, nearPlane, farPlane);
    inverseCurrentViewProjection = MatrixInvert(currentViewProjection);
    const Vector3 cameraForward = Vector3Normalize(Vector3Subtract(
            camera.target, camera.position));
    const Vector3 cameraRight = Vector3Normalize(Vector3CrossProduct(
            cameraForward, camera.up));
    const Vector3 cameraUp = Vector3Normalize(Vector3CrossProduct(
            cameraRight, cameraForward));
    currentFrameState = SectorVolumetricHistoryFrameState{};
    currentFrameState.valid = true;
    currentFrameState.targetWidth = sceneWidth;
    currentFrameState.targetHeight = sceneHeight;
    currentFrameState.quality = resolved.quality;
    currentFrameState.atlas = resolved.atlas;
    currentFrameState.cameraPosition = camera.position;
    currentFrameState.cameraForward = cameraForward;
    currentFrameState.cameraUp = cameraUp;
    currentFrameState.verticalFovDegrees = camera.fovy;
    currentFrameState.aspectRatio = aspectRatio;
    currentFrameState.nearPlane = nearPlane;
    currentFrameState.farPlane = farPlane;
    currentFrameState.projection = camera.projection;
    currentFrameState.renderSeconds = runtimeSeconds;
    currentFrameState.fogSignature = FogSignature(fogSettings);
    currentFrameState.sourceRevision = AtmosphereSourceSignature(
            sourceRevision, map, lightAtmosphereSources);
    const SectorVolumetricHistoryResetReason resetReason =
            EvaluateSectorVolumetricHistoryReset(
                    previousFrameState, currentFrameState);
    if (resetReason != SectorVolumetricHistoryResetReason::None) {
        InvalidateHistory(resetReason);
    }
    const bool hasLocalMedium = std::any_of(
            map.compiledLocalFogVolumes.begin(),
            map.compiledLocalFogVolumes.end(),
            [](const SectorCompiledLocalFogVolume& volume) {
                return volume.enabled && volume.density > 0.0f
                        && volume.maxOpacity > 0.0f;
            });
    const float startDistance = hasLocalMedium
            ? nearPlane
            : std::max(nearPlane, fogSettings.startDistanceWorld);
    const float endDistance = std::min(
            farPlane, fogSettings.volumetricMaxDistanceWorld);
    const SectorVolumetricQualityContract contract =
            GetSectorVolumetricQualityContract(resolved.quality);
    if (!ComputeSectorVolumetricDepthSliceLayout(
                startDistance, endDistance,
                resolved.grid.z, contract.clusterBands,
                depthLayout)
            || !clusterBuilder.Build(
                    map, lightAtmosphereSources, runtimePointLight,
                    dynamicLights, visibility, receiverBounds,
                    camera, aspectRatio, depthLayout,
                    runtimeSeconds, dynamicLightingEnabled)) {
        resourceDiagnostic = "disabled: invalid froxel depth or cluster layout";
        InvalidateHistory(SectorVolumetricHistoryResetReason::ResourceUnavailable);
        return false;
    }
    activeMedia = globalFogActive
            || clusterBuilder.Diagnostics().retainedVolumeCount > 0;
    if (!activeMedia) {
        resourceDiagnostic = "inactive: no visible unified media";
        InvalidateHistory(SectorVolumetricHistoryResetReason::InactiveFrame);
        return false;
    }
    if (!BuildAndUploadDataTextures()) {
        resourcesReady = false;
        resourceDiagnostic = "disabled: froxel data texture upload failed";
        if (!warnedUnavailable) {
            TraceLog(LOG_WARNING,
                    "UNIFIED ATMOSPHERE: froxel upload failed; using analytic fog fallback");
            warnedUnavailable = true;
        }
        InvalidateHistory(SectorVolumetricHistoryResetReason::ResourceUnavailable);
        return false;
    }
    resourcesReady = true;
    return true;
}

void SectorVolumetricAtmosphereRenderer::UploadCommon(
        Shader value,
        const CommonLocations& locations) const
{
    const Vector3 forward = Vector3Normalize(Vector3Subtract(
            preparedCamera.target, preparedCamera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(
            forward, preparedCamera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float tanHalf = std::tan(preparedCamera.fovy * DEG2RAD * 0.5f);
    const Vector3 gridSize{
            static_cast<float>(resourceLayout.grid.x),
            static_cast<float>(resourceLayout.grid.y),
            static_cast<float>(resourceLayout.grid.z)};
    const int sliceCount = depthLayout.sliceCount;
    const int clusterBands = depthLayout.clusterBandCount;
#define SET(field, data, type) SetShaderValue(value, locations.field, &data, type)
    SET(cameraPosition, preparedCamera.position, SHADER_UNIFORM_VEC3);
    SET(cameraForward, forward, SHADER_UNIFORM_VEC3);
    SET(cameraRight, right, SHADER_UNIFORM_VEC3);
    SET(cameraUp, up, SHADER_UNIFORM_VEC3);
    SET(tanHalfFov, tanHalf, SHADER_UNIFORM_FLOAT);
    SET(aspectRatio, aspectRatio, SHADER_UNIFORM_FLOAT);
    SET(gridSize, gridSize, SHADER_UNIFORM_VEC3);
    SET(tileColumns, resourceLayout.atlas.tileColumns, SHADER_UNIFORM_INT);
    SET(sliceCount, sliceCount, SHADER_UNIFORM_INT);
    SET(clusterBandCount, clusterBands, SHADER_UNIFORM_INT);
    SET(runtimeSeconds, preparedRuntimeSeconds, SHADER_UNIFORM_FLOAT);
    SET(jitter, currentJitter, SHADER_UNIFORM_VEC3);
#undef SET
    SetShaderValueV(value, locations.sliceDepths,
            depthLayout.endpoints.data(), SHADER_UNIFORM_FLOAT,
            depthLayout.sliceCount + 1);
}

bool SectorVolumetricAtmosphereRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& sceneScratch)
{
    if (!prepared || !resourcesReady || !activeMedia
            || sceneTarget.texture.id == 0 || sceneTarget.depth.id == 0) {
        InvalidateHistory(SectorVolumetricHistoryResetReason::ResourceUnavailable);
        return false;
    }
    const int fogEnabled = globalFogActive ? 1 : 0;
    const Vector3 fogColor = engine::SrgbColorBytesToLinearSceneRgb(
            fogSettings.color);

    rlDrawRenderBatchActive();
    BeginTextureMode(mediumAtlas.native);
    ClearBackground(BLANK);
    BeginShaderMode(mediumShader);
    SetShaderValueTexture(mediumShader, mediumLocations.volumeData,
            volumeDataTexture);
    SetShaderValueTexture(mediumShader, mediumLocations.volumeLists,
            volumeListTexture);
    UploadCommon(mediumShader, mediumLocations);
#define MSET(field, data, type) SetShaderValue(mediumShader, mediumLocations.field, &data, type)
    MSET(fogEnabled, fogEnabled, SHADER_UNIFORM_INT);
    MSET(fogColor, fogColor, SHADER_UNIFORM_VEC3);
    MSET(fogStartDistance, fogSettings.startDistanceWorld, SHADER_UNIFORM_FLOAT);
    MSET(fogDensity, fogSettings.density, SHADER_UNIFORM_FLOAT);
    MSET(fogMaximumOpacity, fogSettings.maxOpacity, SHADER_UNIFORM_FLOAT);
    MSET(fogReferenceHeight, fogSettings.referenceHeightWorld, SHADER_UNIFORM_FLOAT);
    MSET(fogHeightFalloff, fogSettings.heightFalloff, SHADER_UNIFORM_FLOAT);
#undef MSET
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture),
            DestinationRect(mediumAtlas.native.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

    rlDrawRenderBatchActive();
    BeginTextureMode(lightingAtlas.native);
    ClearBackground(BLANK);
    BeginShaderMode(lightShader);
    SetShaderValueTexture(lightShader, lightLocations.mediumAtlas,
            mediumAtlas.native.texture);
    SetShaderValueTexture(lightShader, lightLocations.lightData,
            lightDataTexture);
    SetShaderValueTexture(lightShader, lightLocations.lightLists,
            lightListTexture);
    UploadCommon(lightShader, lightLocations);
    SetShaderValue(lightShader, lightLocations.anisotropy,
            &fogSettings.anisotropy, SHADER_UNIFORM_FLOAT);
    for (std::size_t index = 0; index < MaxDynamicSpotLightShadowCasters; ++index) {
        if (lightLocations.shadowLightMatrices[index] >= 0) {
            SetShaderValueMatrix(
                    lightShader,
                    lightLocations.shadowLightMatrices[index],
                    preparedDynamicLights.shadowUniforms.shadowLightMatrices[index]);
        }
    }
    SetShaderValueV(lightShader, lightLocations.shadowBias,
            preparedDynamicLights.shadowUniforms.shadowBias.data(),
            SHADER_UNIFORM_FLOAT,
            static_cast<int>(MaxDynamicSpotLightShadowCasters));
    SetShaderValueV(lightShader, lightLocations.shadowStrength,
            preparedDynamicLights.shadowUniforms.shadowStrength.data(),
            SHADER_UNIFORM_FLOAT,
            static_cast<int>(MaxDynamicSpotLightShadowCasters));
    SetShaderValueV(lightShader, lightLocations.shadowSoftness,
            preparedDynamicLights.shadowUniforms.shadowSoftness.data(),
            SHADER_UNIFORM_FLOAT,
            static_cast<int>(MaxDynamicSpotLightShadowCasters));
    if (ValidShadowTexture(preparedDynamicLights.shadowMaps.shadowMap0)) {
        SetShaderValueTexture(lightShader, lightLocations.shadowMap0,
                *preparedDynamicLights.shadowMaps.shadowMap0);
    }
    if (ValidShadowTexture(preparedDynamicLights.shadowMaps.shadowMap1)) {
        SetShaderValueTexture(lightShader, lightLocations.shadowMap1,
                *preparedDynamicLights.shadowMaps.shadowMap1);
    }
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture),
            DestinationRect(lightingAtlas.native.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

    rlDrawRenderBatchActive();
    BeginTextureMode(integratedTarget.native);
    ClearBackground(BLANK);
    BeginShaderMode(integrationShader);
    SetShaderValueTexture(integrationShader, integrationLocations.lightingAtlas,
            lightingAtlas.native.texture);
    SetShaderValueTexture(integrationShader, integrationLocations.sceneDepth,
            sceneTarget.depth);
    UploadCommon(integrationShader, integrationLocations);
    SetShaderValue(integrationShader, integrationLocations.nearPlane,
            &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(integrationShader, integrationLocations.farPlane,
            &farPlane, SHADER_UNIFORM_FLOAT);
    rlDisableColorBlend();
    BeginAlwaysDepthWrite();
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture),
            DestinationRect(integratedTarget.native.texture), Vector2{}, 0.0f, WHITE);
    EndAlwaysDepthWrite();
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

    const engine::RenderTarget* atmosphereTarget = &integratedTarget;
    int historyWriteIndex = 1 - historyReadIndex;
    const bool outputHistoryWeight = temporalPolicy.enabled
            && debugView == SectorVolumetricDebugView::HistoryWeight;
    if (temporalPolicy.enabled) {
        engine::RenderTarget& historyWrite = historyTargets[
                static_cast<std::size_t>(historyWriteIndex)];
        const engine::RenderTarget& historyRead = historyTargets[
                static_cast<std::size_t>(historyReadIndex)];
        const Vector2 texelSize{
                1.0f / integratedTarget.native.texture.width,
                1.0f / integratedTarget.native.texture.height};
        const Vector2 currentJitterUv{
                currentJitter.x * texelSize.x,
                currentJitter.y * texelSize.y};
        const Vector2 previousJitterUv{
                historyJitter.x * texelSize.x,
                historyJitter.y * texelSize.y};
        const int validHistory = historyValid ? 1 : 0;
        const int outputWeight = outputHistoryWeight ? 1 : 0;
        rlDrawRenderBatchActive();
        BeginTextureMode(historyWrite.native);
        ClearBackground(BLANK);
        BeginShaderMode(temporalShader);
        SetShaderValueTexture(temporalShader, temporalLocations.currentAtmosphere,
                integratedTarget.native.texture);
        SetShaderValueTexture(temporalShader, temporalLocations.currentDepth,
                integratedTarget.native.depth);
        SetShaderValueTexture(temporalShader, temporalLocations.historyAtmosphere,
                historyRead.native.texture);
        SetShaderValueTexture(temporalShader, temporalLocations.historyDepth,
                historyRead.native.depth);
        SetShaderValueMatrix(temporalShader,
                temporalLocations.inverseCurrentViewProjection,
                inverseCurrentViewProjection);
        SetShaderValueMatrix(temporalShader,
                temporalLocations.previousViewProjection,
                historyViewProjection);
        SetShaderValue(temporalShader, temporalLocations.currentJitterUv,
                &currentJitterUv, SHADER_UNIFORM_VEC2);
        SetShaderValue(temporalShader, temporalLocations.previousJitterUv,
                &previousJitterUv, SHADER_UNIFORM_VEC2);
        SetShaderValue(temporalShader, temporalLocations.texelSize,
                &texelSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(temporalShader, temporalLocations.nearPlane,
                &nearPlane, SHADER_UNIFORM_FLOAT);
        SetShaderValue(temporalShader, temporalLocations.farPlane,
                &farPlane, SHADER_UNIFORM_FLOAT);
        SetShaderValue(temporalShader, temporalLocations.historyValid,
                &validHistory, SHADER_UNIFORM_INT);
        SetShaderValue(temporalShader, temporalLocations.baseCurrentWeight,
                &temporalPolicy.baseCurrentFrameWeight, SHADER_UNIFORM_FLOAT);
        SetShaderValue(temporalShader, temporalLocations.responsiveCurrentWeight,
                &temporalPolicy.responsiveCurrentFrameWeight, SHADER_UNIFORM_FLOAT);
        SetShaderValue(temporalShader, temporalLocations.outputHistoryWeight,
                &outputWeight, SHADER_UNIFORM_INT);
        rlDisableColorBlend();
        BeginAlwaysDepthWrite();
        DrawTexturePro(integratedTarget.native.texture,
                SourceRect(integratedTarget.native.texture),
                DestinationRect(historyWrite.native.texture),
                Vector2{}, 0.0f, WHITE);
        EndAlwaysDepthWrite();
        rlDrawRenderBatchActive();
        EndShaderMode();
        rlEnableColorBlend();
        EndTextureMode();
        atmosphereTarget = &historyWrite;
    }

    const Vector2 texelSize{
            1.0f / integratedTarget.native.texture.width,
            1.0f / integratedTarget.native.texture.height};
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneScratch);
    ClearBackground(BLANK);
    BeginShaderMode(compositeShader);
    SetShaderValueTexture(compositeShader, compositeLocations.sceneColor,
            sceneTarget.texture);
    SetShaderValueTexture(compositeShader, compositeLocations.sceneDepth,
            sceneTarget.depth);
    SetShaderValueTexture(compositeShader,
            compositeLocations.atmosphereTexture,
            atmosphereTarget->native.texture);
    SetShaderValueTexture(compositeShader,
            compositeLocations.atmosphereDepth,
            atmosphereTarget->native.depth);
    SetShaderValueTexture(compositeShader,
            compositeLocations.mediumAtlas,
            mediumAtlas.native.texture);
    SetShaderValueTexture(compositeShader,
            compositeLocations.lightingAtlas,
            lightingAtlas.native.texture);
    SetShaderValue(compositeShader, compositeLocations.atmosphereTexelSize,
            &texelSize, SHADER_UNIFORM_VEC2);
    const float maximumDistance = depthLayout.endpoints[
            static_cast<std::size_t>(depthLayout.sliceCount)];
    const int debugMode = static_cast<int>(debugView);
    const int historyAvailable = temporalPolicy.enabled ? 1 : 0;
    SetShaderValue(compositeShader, compositeLocations.nearPlane,
            &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeLocations.farPlane,
            &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeLocations.maximumDistance,
            &maximumDistance, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeLocations.debugView,
            &debugMode, SHADER_UNIFORM_INT);
    SetShaderValue(compositeShader, compositeLocations.historyAvailable,
            &historyAvailable, SHADER_UNIFORM_INT);
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture),
            DestinationRect(sceneScratch.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();
    if (temporalPolicy.enabled && !historyFrozen && !outputHistoryWeight) {
        historyReadIndex = historyWriteIndex;
        historyViewProjection = currentViewProjection;
        historyJitter = currentJitter;
        historyValid = true;
        ++historyFrameCount;
    }
    previousFrameState = currentFrameState;
    previousFrameState.valid = true;
    ++temporalFrameIndex;
    return true;
}

void SectorVolumetricAtmosphereRenderer::ReleaseResources()
{
    engine::UnloadRenderTarget(mediumAtlas);
    engine::UnloadRenderTarget(lightingAtlas);
    engine::UnloadRenderTarget(integratedTarget);
    for (engine::RenderTarget& target : historyTargets) {
        engine::UnloadRenderTarget(target);
    }
    if (lightDataTexture.id != 0) UnloadTexture(lightDataTexture);
    if (volumeDataTexture.id != 0) UnloadTexture(volumeDataTexture);
    if (lightListTexture.id != 0) UnloadTexture(lightListTexture);
    if (volumeListTexture.id != 0) UnloadTexture(volumeListTexture);
    lightDataTexture = {};
    volumeDataTexture = {};
    lightListTexture = {};
    volumeListTexture = {};
    clusterBuilder.Clear();
    estimatedResourceBytes = 0;
}

void SectorVolumetricAtmosphereRenderer::ResetPreparedFrame()
{
    prepared = false;
    resourcesReady = false;
    activeMedia = false;
    globalFogActive = false;
    depthLayout = {};
    clusterBuilder.ResetFrame();
}

void SectorVolumetricAtmosphereRenderer::InvalidateHistory(
        SectorVolumetricHistoryResetReason reason)
{
    historyValid = false;
    historyReadIndex = 0;
    historyFrameCount = 0;
    historyViewProjection = MatrixIdentity();
    historyJitter = {};
    if (reason != SectorVolumetricHistoryResetReason::None) {
        historyResetReason = reason;
    }
}

void SectorVolumetricAtmosphereRenderer::SetHistoryFrozen(bool frozen)
{
    if (historyFrozen == frozen) return;
    const bool wasFrozen = historyFrozen;
    historyFrozen = frozen;
    if (wasFrozen && !frozen) {
        InvalidateHistory(SectorVolumetricHistoryResetReason::FreezeReleased);
    }
}

void SectorVolumetricAtmosphereRenderer::SetDebugView(
        SectorVolumetricDebugView view)
{
    if (debugView == view) return;
    const bool leavingHistoryWeight =
            debugView == SectorVolumetricDebugView::HistoryWeight;
    debugView = view;
    if (leavingHistoryWeight) {
        InvalidateHistory(SectorVolumetricHistoryResetReason::DebugViewChanged);
    }
}

void SectorVolumetricAtmosphereRenderer::Shutdown()
{
    ReleaseResources();
    if (mediumShader.id != 0) UnloadShader(mediumShader);
    if (lightShader.id != 0) UnloadShader(lightShader);
    if (integrationShader.id != 0) UnloadShader(integrationShader);
    if (temporalShader.id != 0) UnloadShader(temporalShader);
    if (compositeShader.id != 0) UnloadShader(compositeShader);
    mediumShader = {};
    lightShader = {};
    integrationShader = {};
    temporalShader = {};
    compositeShader = {};
    mediumLocations = {};
    lightLocations = {};
    integrationLocations = {};
    temporalLocations = {};
    compositeLocations = {};
    resourceLayout = {};
    failedSceneWidth = 0;
    failedSceneHeight = 0;
    failedQuality = SectorTopologyFogSettings::VolumetricQuality::Off;
    requestedQuality = SectorTopologyFogSettings::VolumetricQuality::Off;
    maximumTextureSize = 0;
    shaderFailed = false;
    warnedUnavailable = false;
    resourceDiagnostic = "not initialized";
    temporalPolicy = {};
    currentFrameState = {};
    previousFrameState = {};
    currentViewProjection = MatrixIdentity();
    inverseCurrentViewProjection = MatrixIdentity();
    temporalFrameIndex = 0;
    historyFrozen = false;
    debugView = SectorVolumetricDebugView::Composite;
    shadowedSpotLightCount = 0;
    InvalidateHistory(SectorVolumetricHistoryResetReason::RendererReset);
    ResetPreparedFrame();
}

} // namespace game
