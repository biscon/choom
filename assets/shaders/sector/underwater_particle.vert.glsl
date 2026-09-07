#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragUv;
out vec4 fragParticleColor;
void main() {
    fragUv = vertexTexCoord;
    fragParticleColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
