#version 330
in vec2 uv;
in vec4 color;
in float emissive;
in float softDistance;
uniform sampler2D particleTexture;
uniform sampler2D sceneDepth;
uniform int hasDepth;
uniform int displaySrgb;
uniform vec2 viewportSize;
uniform vec2 nearFar;
out vec4 finalColor;
float linearDepth(float z) {
    return 2.0 * nearFar.x * nearFar.y / (nearFar.y + nearFar.x - (z * 2.0 - 1.0) * (nearFar.y - nearFar.x));
}
void main() {
    vec4 texel = texture(particleTexture, uv);
    float particleDepth = linearDepth(gl_FragCoord.z);
    float fade = smoothstep(nearFar.x, nearFar.x + 0.15, particleDepth);
    if (hasDepth != 0) {
        float scene = linearDepth(texture(sceneDepth, gl_FragCoord.xy / viewportSize).r);
        fade *= clamp((scene - particleDepth) / softDistance, 0.0, 1.0);
    }
    float alpha = texel.a * color.a * fade;
    vec3 radiance = min(texel.rgb * color.rgb, vec3(65000.0));
    if (displaySrgb != 0) radiance = pow(radiance / (radiance + vec3(1.0)), vec3(1.0 / 2.2));
    finalColor = vec4(radiance * alpha, alpha * (1.0 - emissive));
}
