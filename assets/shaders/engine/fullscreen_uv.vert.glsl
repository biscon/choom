#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragUv;
uniform mat4 mvp;
void main() { fragUv = vertexTexCoord; gl_Position = mvp * vec4(vertexPosition, 1.0); }
