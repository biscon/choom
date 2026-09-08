#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 lightViewProjection;
uniform mat4 matModel;
uniform int useSkinning;
#define MAX_BONE_NUM 128
uniform mat4 boneMatrices[MAX_BONE_NUM];

out vec2 fragTexCoord;

void main()
{
    vec4 localPosition = vec4(vertexPosition, 1.0);
    if (useSkinning != 0) {
        int bone0 = int(vertexBoneIndices.x);
        int bone1 = int(vertexBoneIndices.y);
        int bone2 = int(vertexBoneIndices.z);
        int bone3 = int(vertexBoneIndices.w);
        localPosition = vertexBoneWeights.x * (boneMatrices[bone0] * localPosition)
                + vertexBoneWeights.y * (boneMatrices[bone1] * localPosition)
                + vertexBoneWeights.z * (boneMatrices[bone2] * localPosition)
                + vertexBoneWeights.w * (boneMatrices[bone3] * localPosition);
    }
    fragTexCoord = vertexTexCoord;
    gl_Position = lightViewProjection * matModel * localPosition;
}
