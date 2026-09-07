#include "sector_demo/renderer/SectorReflectionSampling.h"
#include "sector_demo/renderer/SectorShaderSource.h"
#include "engine/assets/AssetManager.h"
#include <rlgl.h>
#include <external/glad.h>
#include <algorithm>

namespace game
{
namespace
{
const char *ReflectionSource = R"(
uniform samplerCube rpCurrent0;
uniform samplerCube rpPrevious0;
uniform samplerCube rpCurrent1;
uniform samplerCube rpPrevious1;
uniform vec3 rpCapture[2];
uniform vec3 rpCenter[2];
uniform vec3 rpExtents[2];
// yaw, intensity, maxLod, box projection
uniform vec4 rpParameters[2];
uniform vec2 rpTransition;
uniform vec2 rpWidths;
uniform vec4 rpPlane;
uniform vec4 rpAperture;
uniform vec2 rpHeights;
uniform int rpMode;
vec3 ReflectionLocal(vec3 p, float yaw) {
    float c=cos(yaw), s=sin(yaw);
    return vec3(p.x*c-p.z*s,p.y,p.x*s+p.z*c);
}
vec3 ReflectionDirection(int i, vec3 position, vec3 direction) {
    if (rpParameters[i].w < 0.5) return direction;
    vec3 origin=ReflectionLocal(position-rpCenter[i],-rpParameters[i].x);
    vec3 ray=ReflectionLocal(direction,-rpParameters[i].x);
    vec3 safeRay=mix(vec3(-1),vec3(1),step(vec3(0),ray))*max(abs(ray),vec3(0.00001));
    vec3 plane=mix(-rpExtents[i],rpExtents[i],step(vec3(0),ray));
    vec3 distance=(plane-origin)/safeRay;
    vec3 hit=origin+ray*max(min(distance.x,min(distance.y,distance.z)),0.0);
    vec3 lookup=hit-ReflectionLocal(rpCapture[i]-rpCenter[i],-rpParameters[i].x);
    if (dot(lookup,lookup)<0.000001) return direction;
    return normalize(ReflectionLocal(lookup,rpParameters[i].x));
}
float ReflectionWeight(int i, vec3 position) {
    if (rpParameters[i].y<=0.0) return 0.0;
    if (rpParameters[i].w<0.5) return 0.0001;
    vec3 local=abs(ReflectionLocal(position-rpCenter[i],-rpParameters[i].x));
    vec3 edge=rpExtents[i]-local;
    float d=min(edge.x,min(edge.y,edge.z));
    return rpWidths[i]>0.0 ? clamp(d/rpWidths[i],0.0,1.0) : step(0.0,d);
}
vec3 SampleSectorEnvironment(vec3 position, vec3 direction, float roughness) {
    if (rpMode==0) return vec3(0);
    vec3 d0=ReflectionDirection(0,position,direction);
    float l0=roughness*rpParameters[0].z;
    vec3 a=textureLod(rpCurrent0,d0,l0).rgb;
    if (rpTransition.x<1.0) a=mix(textureLod(rpPrevious0,d0,l0).rgb,a,rpTransition.x);
    a*=rpParameters[0].y;
    if (rpMode==1) return a;
    vec3 d1=ReflectionDirection(1,position,direction);
    float l1=roughness*rpParameters[1].z;
    vec3 b=textureLod(rpCurrent1,d1,l1).rgb;
    if (rpTransition.y<1.0) b=mix(textureLod(rpPrevious1,d1,l1).rgb,b,rpTransition.y);
    b*=rpParameters[1].y;
    float weight;
    if (rpMode==3) {
        float along=dot(position.xz,rpAperture.xy)-rpAperture.z;
        if (along<0.0 || along>rpAperture.w || position.y<rpHeights.x || position.y>rpHeights.y) return a;
        float d=dot(rpPlane.xyz,position)+rpPlane.w;
        weight=smoothstep(-max(rpWidths.x,0.00001),max(rpWidths.y,0.00001),d);
    } else {
        float wa=ReflectionWeight(0,position), wb=ReflectionWeight(1,position);
        weight=wa+wb>0.00001 ? wb/(wa+wb) : 0.0;
    }
    return mix(a,b,weight);
}
)";
}

std::string AddSectorReflectionShaderSource(const char *source)
{
    return InsertSectorShaderPreamble(source, ReflectionSource);
}

SectorReflectionShaderLocations LoadSectorReflectionShaderLocations(Shader shader)
{
    SectorReflectionShaderLocations l;
    const char *names[]{"rpCurrent0", "rpPrevious0", "rpCurrent1", "rpPrevious1"};
    for (int i = 0; i < 4; ++i)
    {
        l.textures[i] = GetShaderLocation(shader, names[i]);
        const int unit = 12 + i;
        SetShaderValue(shader, l.textures[i], &unit, SHADER_UNIFORM_INT);
    }
    l.capture = GetShaderLocation(shader, "rpCapture[0]");
    l.center = GetShaderLocation(shader, "rpCenter[0]");
    l.extents = GetShaderLocation(shader, "rpExtents[0]");
    l.parameters = GetShaderLocation(shader, "rpParameters[0]");
    l.transition = GetShaderLocation(shader, "rpTransition");
    l.widths = GetShaderLocation(shader, "rpWidths");
    l.plane = GetShaderLocation(shader, "rpPlane");
    l.mode = GetShaderLocation(shader, "rpMode");
    l.aperture = GetShaderLocation(shader, "rpAperture");
    l.heights = GetShaderLocation(shader, "rpHeights");
    return l;
}

void UploadSectorReflectionBlend(Shader shader, const SectorReflectionShaderLocations &l,
                                 const SectorPbrEnvironmentBlend &blend,
                                 engine::AssetManager &assets, float skyExposure)
{
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    const SectorPbrEnvironmentSelection sources[]{blend.first, blend.second};
    Vector3 capture[2], center[2], extents[2];
    Vector4 parameters[2];
    float transitions[2], widths[2];
    bool ready[2]{};
    for (int i = 0; i < 2; ++i)
    {
        const auto &s = sources[i];
        const TextureCubemap *current = assets.GetCubemap(s.cubemap);
        const TextureCubemap *previous = assets.GetCubemap(s.previous);
        ready[i] = current && current->id;
        capture[i] = s.capturePosition;
        center[i] = s.influenceCenter;
        extents[i] = s.halfExtents;
        parameters[i] = {s.yawRadians,
                         ready[i] ? s.intensity * (s.localProbe ? 1.0f : skyExposure) : 0.0f,
                         s.maxLod, s.boxProjection ? 1.0f : 0.0f};
        transitions[i] = previous ? s.transition : 1.0f;
        widths[i] = s.blendDistance;
        rlActiveTextureSlot(12 + i * 2);
        rlEnableTextureCubemap(ready[i] ? current->id : 0);
        rlActiveTextureSlot(13 + i * 2);
        rlEnableTextureCubemap(previous ? previous->id : (ready[i] ? current->id : 0));
    }
    rlActiveTextureSlot(0);
    const int mode = !ready[0] ? 0 : !ready[1] ? 1 : blend.portal ? 3 : 2;
    if (blend.portal)
    {
        widths[0] = blend.portalWidths.x;
        widths[1] = blend.portalWidths.y;
    }
    SetShaderValueV(shader, l.capture, capture, SHADER_UNIFORM_VEC3, 2);
    SetShaderValueV(shader, l.center, center, SHADER_UNIFORM_VEC3, 2);
    SetShaderValueV(shader, l.extents, extents, SHADER_UNIFORM_VEC3, 2);
    SetShaderValueV(shader, l.parameters, parameters, SHADER_UNIFORM_VEC4, 2);
    SetShaderValue(shader, l.transition, transitions, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, l.widths, widths, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, l.plane, &blend.portalPlane, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, l.aperture, &blend.portalAperture, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, l.heights, &blend.portalHeights, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, l.mode, &mode, SHADER_UNIFORM_INT);
}
} // namespace game
