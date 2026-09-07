#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 texelSize;
uniform vec2 direction;
uniform float radius;
#include "../engine/hdr_storage.glsl"
void main() {
    vec2 offset = direction * texelSize * radius;
    vec3 color = texture(texture0, fragTexCoord).rgb * 0.227027;
    color += texture(texture0, fragTexCoord + offset * 1.384615).rgb * 0.316216;
    color += texture(texture0, fragTexCoord - offset * 1.384615).rgb * 0.316216;
    color += texture(texture0, fragTexCoord + offset * 3.230769).rgb * 0.070270;
    color += texture(texture0, fragTexCoord - offset * 3.230769).rgb * 0.070270;
    finalColor = vec4(SanitizeLinearHdrForRgba16f(color), 0.0);
}
