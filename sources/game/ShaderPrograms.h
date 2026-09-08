#pragma once

#include "engine/render/ShaderSource.h"
#include <array>

namespace game {

// Program metadata only. GLSL source lives under assets/shaders/.
enum class GameShader {
    BloomPrefilter,
    BloomBlur,
    BloomComposite,
    DistanceFog,
    LightDust,
    DynamicModelShadowContact,
    BillboardCutout,
    Caustics,
    UnderwaterParticle,
    AnalyticFog,
    AnalyticShaft,
    AnalyticHalo,
    DoorOpaque,
    SpotLightShadowOpaque,
    SpotLightShadowCutout,
    Liquid,
    Lightmap,
    DepthPrepass,
    HdrComposite,
    ReflectionFilter,
    StaticModel,
    Window,
    MuzzleFlash,
    Icon,
    Fxaa,
    ScenePresentation,
    WindowFlatTransmission,
    WindowFlatReflection,
    Count
};

inline constexpr engine::ShaderDefine WindowDefaultDefines[]{{"WINDOW_FLAT_PASS", "0"}};
inline constexpr engine::ShaderDefine WindowTransmissionDefines[]{{"WINDOW_FLAT_PASS", "1"}};
inline constexpr engine::ShaderDefine WindowReflectionDefines[]{{"WINDOW_FLAT_PASS", "2"}};

inline constexpr std::array<engine::ShaderProgramDefinition,
        static_cast<std::size_t>(GameShader::Count)> GameShaderPrograms{{
    {"bloom_prefilter", nullptr, "sector/bloom_prefilter.frag.glsl"},
    {"bloom_blur", nullptr, "sector/bloom_blur.frag.glsl"},
    {"bloom_composite", nullptr, "sector/bloom_composite.frag.glsl"},
    {"distance_fog", "engine/fullscreen_uv.vert.glsl", "sector/distance_fog.frag.glsl"},
    {"light_dust", "sector/light_dust.vert.glsl", "sector/light_dust.frag.glsl"},
    {"dynamic_model_shadow_contact", "sector/dynamic_model_shadow_contact.vert.glsl", "sector/dynamic_model_shadow_contact.frag.glsl"},
    {"billboard_cutout", "sector/billboard_cutout.vert.glsl", "sector/billboard_cutout.frag.glsl"},
    {"caustics", "engine/fullscreen_uv.vert.glsl", "sector/caustics.frag.glsl"},
    {"underwater_particle", "sector/underwater_particle.vert.glsl", "sector/underwater_particle.frag.glsl"},
    {"analytic_fog", "engine/fullscreen_triangle.vert.glsl", "sector/analytic_fog.frag.glsl"},
    {"analytic_shaft", "engine/fullscreen_triangle.vert.glsl", "sector/analytic_shaft.frag.glsl"},
    {"analytic_halo", "engine/fullscreen_triangle.vert.glsl", "sector/analytic_halo.frag.glsl"},
    {"door_opaque", "sector/door_opaque.vert.glsl", "sector/door_opaque.frag.glsl"},
    {"spot_light_shadow_opaque", "sector/spot_light_shadow.vert.glsl", "sector/spot_light_shadow_opaque.frag.glsl"},
    {"spot_light_shadow_cutout", "sector/spot_light_shadow.vert.glsl", "sector/spot_light_shadow_cutout.frag.glsl"},
    {"liquid", "sector/liquid.vert.glsl", "sector/liquid.frag.glsl"},
    {"lightmap", "sector/lightmap.vert.glsl", "sector/lightmap.frag.glsl"},
    {"depth_prepass", "sector/depth_prepass.vert.glsl", "sector/depth_prepass.frag.glsl"},
    {"hdr_composite", "engine/fullscreen_uv.vert.glsl", "sector/hdr_composite.frag.glsl"},
    {"reflection_filter", "sector/reflection_filter.vert.glsl", "sector/reflection_filter.frag.glsl"},
    {"static_model", "sector/static_model.vert.glsl", "sector/static_model.frag.glsl"},
    {"window", "sector/window.vert.glsl", "sector/window.frag.glsl", WindowDefaultDefines, 1},
    {"muzzle_flash", "game/muzzle_flash.vert.glsl", "game/muzzle_flash.frag.glsl"},
    {"icon", "game/icon.vert.glsl", "game/icon.frag.glsl"},
    {"fxaa", nullptr, "engine/fxaa.frag.glsl"},
    {"scene_presentation", nullptr, "engine/scene_presentation.frag.glsl"},
    {"window_flat_transmission", "sector/window.vert.glsl", "sector/window.frag.glsl", WindowTransmissionDefines, 1},
    {"window_flat_reflection", "sector/window.vert.glsl", "sector/window.frag.glsl", WindowReflectionDefines, 1},
}};

inline constexpr GameShader WindowShaderVariant(int variant)
{
    return variant == 1 ? GameShader::WindowFlatTransmission
            : variant == 2 ? GameShader::WindowFlatReflection : GameShader::Window;
}

} // namespace game
