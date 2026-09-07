#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform float nearPlane;
uniform float farPlane;
uniform float startDistance;
uniform float endDistance;
uniform float falloffExponent;
uniform float maxOpacity;
uniform vec3 fogColor;
void main() {
    vec4 scene = texture(sceneColor, fragUv);
    float depth = texture(sceneDepth, fragUv).r;
    if (depth >= 0.999999) { finalColor = scene; return; }
    float zNdc = depth * 2.0 - 1.0;
    float distance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float fogRange = max(endDistance - startDistance, 0.0001);
    float factor = pow(clamp((distance - startDistance) / fogRange, 0.0, 1.0),
            max(falloffExponent, 0.0001)) * clamp(maxOpacity, 0.0, 1.0);
    vec3 result = max(scene.rgb, vec3(0.0)) * (1.0 - factor)
            + max(fogColor, vec3(0.0)) * factor;
    finalColor = vec4(min(result, vec3(65504.0)), scene.a);
}
