#version 330
in vec3 vertexPosition;
void main() { gl_Position = vec4(vertexPosition.xy, 0.0, 1.0); }
