#include "sector_demo/renderer/SectorReflectionSampling.h"
#include "engine/assets/AssetManager.h"
#include <rlgl.h>
#include <external/glad.h>
#include <algorithm>

namespace game
{

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
