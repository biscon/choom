#version 330
in vec2 texCoord;
uniform float opacity;
out vec4 finalColor;
void main()
{
    vec2 p = texCoord * 2.0 - 1.0;
    float radius = dot(p, p);
    float alpha = (1.0 - smoothstep(0.35, 1.0, radius)) * opacity;
    if (alpha <= 0.002) discard;
    finalColor = vec4(0.0, 0.0, 0.0, alpha);
}
