#version 330
layout(location=0) in vec4 cornerUv;
layout(location=1) in vec4 instancePosition;
layout(location=2) in vec4 instanceRight;
layout(location=3) in vec4 instanceUp;
layout(location=4) in vec4 instanceColor;
layout(location=5) in vec4 instanceUv;
uniform mat4 mvp;
out vec2 uv;
out vec4 color;
out float emissive;
out float softDistance;
void main() {
    vec3 position = instancePosition.xyz + instanceRight.xyz * cornerUv.x + instanceUp.xyz * cornerUv.y;
    gl_Position = mvp * vec4(position, 1.0);
    uv = instanceUv.xy + cornerUv.zw * instanceUv.zw;
    color = vec4(instanceColor.rgb, instanceUp.w);
    emissive = instanceRight.w;
    softDistance = max(0.03, instancePosition.w * 0.5);
}
