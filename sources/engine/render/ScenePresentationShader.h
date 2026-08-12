#pragma once

#include "engine/render/ColorTransferGlsl.h"
#include "engine/render/ToneMappingGlsl.h"

#include <string>

namespace engine {

inline std::string BuildScenePresentationFragmentShader()
{
    return std::string(R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;

out vec4 finalColor;
)glsl")
            + ToneMappingGlsl
            + ColorTransferGlsl
            + R"glsl(
void main()
{
    vec4 scene = texture(texture0, fragTexCoord);
    vec3 mapped = ToneMapNeutralMaxChannel(scene.rgb);
    finalColor = vec4(
            LinearSceneToDisplaySrgb(mapped),
            clamp(scene.a, 0.0, 1.0)) * fragColor;
}
)glsl";
}

} // namespace engine
