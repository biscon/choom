#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec3 fragLocalPosition;
out vec3 fragLocalNormal;
out vec3 fragWorldAxisX;
out vec3 fragWorldAxisY;
out vec3 fragWorldAxisZ;
void main()
{
    fragWorldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragWorldNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    fragLocalPosition = vertexPosition;
    fragLocalNormal = vertexNormal;
    fragWorldAxisX = normalize((matModel * vec4(1.0, 0.0, 0.0, 0.0)).xyz);
    fragWorldAxisY = normalize((matModel * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
    fragWorldAxisZ = normalize((matModel * vec4(0.0, 0.0, 1.0, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
