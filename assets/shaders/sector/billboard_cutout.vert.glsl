#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragWorldPosition = vertexPosition;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
