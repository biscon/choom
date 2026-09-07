#version 330
in vec2 fragUv;
in vec4 fragParticleColor;
out vec4 finalColor;
uniform vec3 particulateTint;
void main() {
    vec2 centered = fragUv * 2.0 - 1.0;
    float radiusSquared = dot(centered, centered);
    if (radiusSquared >= 1.0) discard;
    float mask = 1.0 - smoothstep(0.08, 1.0, radiusSquared);
    mask *= mask;
    float alpha = fragParticleColor.a * mask;
    finalColor = vec4(particulateTint * alpha, alpha);
}
