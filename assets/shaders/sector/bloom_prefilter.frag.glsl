#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 sourceTexelSize;
uniform float threshold;
uniform float softKnee;
#include "../engine/hdr_storage.glsl"
vec3 Prefilter(vec3 inputColor) {
    vec3 color = SanitizeLinearHdrForRgba16f(inputColor);
    float brightness = max(max(color.r, color.g), color.b);
    if (brightness <= 0.0) return vec3(0.0);
    if (threshold <= 0.0) return color;
    float excess = 0.0;
    if (softKnee <= 0.0) {
        excess = max(brightness - threshold, 0.0);
    } else {
        float knee = threshold * softKnee;
        float q = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
        float softExcess = q * q / (4.0 * knee);
        excess = max(softExcess, max(brightness - threshold, 0.0));
    }
    return SanitizeLinearHdrForRgba16f(color * (excess / brightness));
}
void main() {
    vec3 seed = vec3(0.0);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec2 offset = (vec2(float(x), float(y)) - vec2(1.5)) * sourceTexelSize;
            seed += Prefilter(texture(texture0, clamp(fragTexCoord + offset,
                    vec2(0.0), vec2(1.0))).rgb);
        }
    }
    finalColor = vec4(SanitizeLinearHdrForRgba16f(seed * (1.0 / 16.0)), 0.0);
}
