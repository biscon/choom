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
 vec2 ndc=(vec2(cell)+vec2(0.5))/vec2(grid.xy)*2.0-1.0;vec3 ray=cameraForward+cameraRight*ndc.x*tanHalfFov*aspectRatio+cameraUp*ndc.y*tanHalfFov;
 float viewDepth=sqrt(sliceDepths[z]*sliceDepths[z+1]);vec3 p=cameraPosition+ray*viewDepth;float extinction=0.0;vec3 weightedTint=vec3(0);
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
uniform float anisotropy;
const float kPi=3.14159265358979323846;
vec3 safeNormalize(vec3 v,vec3 fallback){float l2=dot(v,v);return l2>0.00000001?v*inversesqrt(l2):fallback;}
float phaseHg(float cosineTheta){float g=clamp(anisotropy,-0.90,0.90);float d=max(1.0+g*g-2.0*g*clamp(cosineTheta,-1.0,1.0),0.000001);return (1.0-g*g)/(4.0*kPi*d*sqrt(d));}
int listIndex(ivec2 cell,int band,int slot,ivec2 grid){int word=slot/4;int channel=slot-word*4;vec4 encodedIndices=texelFetch(lightLists,ivec2(cell.x*4+word,band*grid.y+cell.y),0);return int(floor(encodedIndices[channel]*255.0+0.5));}
void main(){
 ivec3 grid=ivec3(gridSize+vec3(0.5));ivec2 pixel=ivec2(gl_FragCoord.xy);ivec2 tile=pixel/grid.xy;int z=tile.y*tileColumns+tile.x;if(z<0||z>=sliceCount){finalColor=vec4(0);return;}ivec2 cell=pixel-tile*grid.xy;vec4 medium=texelFetch(mediumAtlas,pixel,0);if(medium.a<=0.0){finalColor=vec4(0);return;}int band=z/8;
 vec2 ndc=(vec2(cell)+vec2(0.5))/vec2(grid.xy)*2.0-1.0;vec3 ray=cameraForward+cameraRight*ndc.x*tanHalfFov*aspectRatio+cameraUp*ndc.y*tanHalfFov;vec3 rayDirection=normalize(ray);float viewDepth=sqrt(sliceDepths[z]*sliceDepths[z+1]);vec3 p=cameraPosition+ray*viewDepth;vec3 direct=vec3(0);
 for(int slot=0;slot<16;++slot){int index=listIndex(cell,band,slot,grid.xy);if(index==255)break;if(index<0||index>=254)continue;vec4 a=texelFetch(lightData,ivec2(0,index),0),b=texelFetch(lightData,ivec2(1,index),0),c=texelFetch(lightData,ivec2(2,index),0),d=texelFetch(lightData,ivec2(3,index),0);vec3 toLight=a.xyz-p;float distance2=dot(toLight,toLight);float range=c.w;if(range<=0.0||distance2>=range*range)continue;float distanceToLight=sqrt(max(distance2,0.0));vec3 lightDirection=distanceToLight>0.0001?toLight/distanceToLight:vec3(0,1,0);float attenuation=clamp(1.0-distanceToLight/range,0.0,1.0);attenuation*=attenuation;float cone=1.0;if(a.w>0.5){vec3 spot=safeNormalize(c.xyz,vec3(0,-1,0));float coneDot=dot(spot,distanceToLight>0.0001?-lightDirection:spot);cone=abs(d.x-d.y)>0.0001?smoothstep(d.y,d.x,coneDot):step(d.x,coneDot);}direct+=b.rgb*b.a*d.z*attenuation*cone*phaseHg(dot(lightDirection,-rayDirection));}
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
uniform float nearPlane;
uniform float farPlane;
float finiteHalf(float v){if(isnan(v))return 0.0;if(isinf(v))return v>0.0?65504.0:0.0;return clamp(v,0.0,65504.0);}
vec3 finiteRadiance(vec3 v){return vec3(finiteHalf(v.r),finiteHalf(v.g),finiteHalf(v.b));}
void main(){
 ivec3 grid=ivec3(gridSize+vec3(0.5));ivec2 cell=ivec2(gl_FragCoord.xy);vec2 ndc=(vec2(cell)+vec2(0.5))/vec2(grid.xy)*2.0-1.0;vec3 ray=cameraForward+cameraRight*ndc.x*tanHalfFov*aspectRatio+cameraUp*ndc.y*tanHalfFov;vec3 rayDirection=normalize(ray);float cosineForward=max(dot(rayDirection,cameraForward),0.0001);float depth=texture(sceneDepth,fragUv).r;float sceneForward=sliceDepths[sliceCount];if(!isnan(depth)&&!isinf(depth)&&depth<0.999999){float zNdc=clamp(depth,0.0,1.0)*2.0-1.0;sceneForward=min(sceneForward,(2.0*nearPlane*farPlane)/max(farPlane+nearPlane-zNdc*(farPlane-nearPlane),0.00001));}
 float transmittance=1.0;vec3 radiance=vec3(0);for(int z=0;z<64;++z){if(z>=sliceCount||sliceDepths[z]>=sceneForward||transmittance<=0.0001)break;float segmentEnd=min(sliceDepths[z+1],sceneForward);float lengthWorld=max(segmentEnd-sliceDepths[z],0.0)/cosineForward;if(lengthWorld<=0.0)continue;ivec2 atlas=ivec2((z%tileColumns)*grid.x+cell.x,(z/tileColumns)*grid.y+cell.y);vec4 lighting=texelFetch(lightingAtlas,atlas,0);float sigma=max(lighting.a,0.0);if(sigma<=0.0)continue;float stepTransmittance=exp(-sigma*lengthWorld);float stepOpacity=1.0-stepTransmittance;vec3 source=max(lighting.rgb,vec3(0))/max(sigma,0.000001);radiance+=transmittance*stepOpacity*source;transmittance*=stepTransmittance;}
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
float finiteHalf(float v){if(isnan(v))return 0.0;if(isinf(v))return v>0.0?65504.0:0.0;return clamp(v,0.0,65504.0);}
vec3 finiteRadiance(vec3 v){return vec3(finiteHalf(v.r),finiteHalf(v.g),finiteHalf(v.b));}
void main(){float centerDepth=texture(sceneDepth,fragUv).r;vec4 atmosphere=vec4(0);float totalWeight=0.0;for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){if(x!=0&&y!=0)continue;vec2 uv=clamp(fragUv+vec2(x,y)*atmosphereTexelSize,vec2(0),vec2(1));float sampleDepth=texture(sceneDepth,uv).r;float difference=(isnan(centerDepth)||isinf(centerDepth)||isnan(sampleDepth)||isinf(sampleDepth))?0.0:abs(sampleDepth-centerDepth);float weight=exp(-difference*600.0);atmosphere+=texture(atmosphereTexture,uv)*weight;totalWeight+=weight;}atmosphere/=max(totalWeight,0.0001);vec4 scene=texture(sceneColor,fragUv);float opacity=(isnan(atmosphere.a)||isinf(atmosphere.a))?0.0:clamp(atmosphere.a,0.0,1.0);vec3 composed=finiteRadiance(scene.rgb)*(1.0-opacity)+finiteRadiance(atmosphere.rgb);float alpha=(isnan(scene.a)||isinf(scene.a))?1.0:clamp(scene.a,0.0,1.0);finalColor=vec4(finiteRadiance(composed),alpha);}
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
#undef LOC
}

bool SectorVolumetricAtmosphereRenderer::Initialize()
{
    if (mediumShader.id != 0 && lightShader.id != 0
            && integrationShader.id != 0 && compositeShader.id != 0) {
        return true;
    }
    if (shaderFailed) return false;
    mediumShader = LoadShaderFromMemory(FullscreenVs, MediumInjectionFs);
    lightShader = LoadShaderFromMemory(FullscreenVs, LightInjectionFs);
    integrationShader = LoadShaderFromMemory(FullscreenVs, IntegrateFs);
    compositeShader = LoadShaderFromMemory(FullscreenVs, CompositeFs);
    if (mediumShader.id == 0 || lightShader.id == 0
            || integrationShader.id == 0 || compositeShader.id == 0) {
        if (mediumShader.id != 0) UnloadShader(mediumShader);
        if (lightShader.id != 0) UnloadShader(lightShader);
        if (integrationShader.id != 0) UnloadShader(integrationShader);
        if (compositeShader.id != 0) UnloadShader(compositeShader);
        mediumShader = {};
        lightShader = {};
        integrationShader = {};
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
#undef LLOC
#define ILOC(field, name) integrationLocations.field = GetShaderLocation(integrationShader, name)
    ILOC(lightingAtlas, "lightingAtlas");
    ILOC(sceneDepth, "sceneDepth");
    ILOC(nearPlane, "nearPlane");
    ILOC(farPlane, "farPlane");
#undef ILOC
    compositeLocations.sceneColor = GetShaderLocation(compositeShader, "sceneColor");
    compositeLocations.sceneDepth = GetShaderLocation(compositeShader, "sceneDepth");
    compositeLocations.atmosphereTexture = GetShaderLocation(compositeShader, "atmosphereTexture");
    compositeLocations.atmosphereTexelSize = GetShaderLocation(compositeShader, "atmosphereTexelSize");
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
                               engine::RenderTarget& target,
                               std::string& error) {
        return engine::LoadRenderTarget(
                engine::RenderTargetDescriptor{
                        name, width, height,
                        engine::RenderTargetColorFormat::Rgba16Float,
                        filter, engine::RenderTargetWrap::Clamp,
                        engine::RenderTargetDepthKind::None, 1},
                target, &error);
    };
    std::string error;
    if (!loadTarget("volumetric-medium-atlas", layout.atlas.width,
                    layout.atlas.height, engine::RenderTargetFilter::Point,
                    mediumAtlas, error)
            || !loadTarget("volumetric-lighting-atlas", layout.atlas.width,
                    layout.atlas.height, engine::RenderTargetFilter::Point,
                    lightingAtlas, error)
            || !loadTarget("volumetric-integrated", layout.integratedWidth,
                    layout.integratedHeight, engine::RenderTargetFilter::Bilinear,
                    integratedTarget, error)
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
    warnedUnavailable = false;
    return true;
}

bool SectorVolumetricAtmosphereRenderer::BuildAndUploadDataTextures()
{
    lightDataStaging.fill(0.0f);
    volumeDataStaging.fill(0.0f);
    const auto& lights = clusterBuilder.Lights();
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
        lightDataStaging[base + 15] = static_cast<float>(light.shadowSlot);
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
        bool dynamicLightingEnabled)
{
    ResetPreparedFrame();
    prepared = true;
    requestedQuality = quality;
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
    if (!Initialize()) return false;
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
        return false;
    }
    failedSceneWidth = 0;
    failedSceneHeight = 0;
    failedQuality = SectorTopologyFogSettings::VolumetricQuality::Off;
    aspectRatio = static_cast<float>(sceneWidth) / std::max(sceneHeight, 1);
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
        return false;
    }
    activeMedia = globalFogActive
            || clusterBuilder.Diagnostics().retainedVolumeCount > 0;
    if (!activeMedia) {
        resourceDiagnostic = "inactive: no visible unified media";
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
    DrawTexturePro(sceneTarget.texture, SourceRect(sceneTarget.texture),
            DestinationRect(integratedTarget.native.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

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
            integratedTarget.native.texture);
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

void SectorVolumetricAtmosphereRenderer::ReleaseResources()
{
    engine::UnloadRenderTarget(mediumAtlas);
    engine::UnloadRenderTarget(lightingAtlas);
    engine::UnloadRenderTarget(integratedTarget);
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

void SectorVolumetricAtmosphereRenderer::Shutdown()
{
    ReleaseResources();
    if (mediumShader.id != 0) UnloadShader(mediumShader);
    if (lightShader.id != 0) UnloadShader(lightShader);
    if (integrationShader.id != 0) UnloadShader(integrationShader);
    if (compositeShader.id != 0) UnloadShader(compositeShader);
    mediumShader = {};
    lightShader = {};
    integrationShader = {};
    compositeShader = {};
    mediumLocations = {};
    lightLocations = {};
    integrationLocations = {};
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
    ResetPreparedFrame();
}

} // namespace game
