#version 330
in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform int alphaTest;
uniform float alphaCutoff;

void main()
{
    if (alphaTest != 0 && texture(texture0, fragTexCoord).a < alphaCutoff) {
        discard;
    }
}
