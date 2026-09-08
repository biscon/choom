#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec4 vertexTangent;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragWorldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragWorldNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    fragColor = vec4(vertexTangent.xyz, 1.0);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
