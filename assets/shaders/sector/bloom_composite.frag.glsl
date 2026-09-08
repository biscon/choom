#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform float intensity;
uniform int bloomOnly;
#include "../engine/hdr_storage.glsl"
float SafeAlpha(float value) {
    return (isnan(value) || isinf(value)) ? 1.0 : clamp(value, 0.0, 1.0);
}
void main() {
    vec3 bloom = texture(bloomTexture, fragTexCoord).rgb;
    if (bloomOnly != 0) {
        finalColor = vec4(SanitizeLinearHdrForRgba16f(bloom * intensity), 0.0);
        return;
    }
    vec4 scene = texture(texture0, fragTexCoord);
    vec3 rgb = scene.rgb + bloom * intensity;
    finalColor = vec4(SanitizeLinearHdrForRgba16f(rgb), SafeAlpha(scene.a));
}
