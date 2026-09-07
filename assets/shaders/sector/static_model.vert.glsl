#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexTangent;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec4 vertexColor;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform vec4 lightmapScaleBias;
uniform int useSkinning;
#define MAX_BONE_NUM 128
uniform mat4 boneMatrices[MAX_BONE_NUM];

out vec2 fragTexCoord;
out vec2 fragLightmapTexCoord;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec3 fragWorldTangent;
out float fragTangentSign;
out vec4 fragColor;

void main()
{
    vec4 localPosition = vec4(vertexPosition, 1.0);
    vec4 localNormal = vec4(vertexNormal, 0.0);
    vec4 localTangent = vec4(vertexTangent.xyz, 0.0);
    if (useSkinning != 0) {
        int bone0 = int(vertexBoneIndices.x);
        int bone1 = int(vertexBoneIndices.y);
        int bone2 = int(vertexBoneIndices.z);
        int bone3 = int(vertexBoneIndices.w);
        localPosition = vertexBoneWeights.x * (boneMatrices[bone0] * localPosition)
                + vertexBoneWeights.y * (boneMatrices[bone1] * localPosition)
                + vertexBoneWeights.z * (boneMatrices[bone2] * localPosition)
                + vertexBoneWeights.w * (boneMatrices[bone3] * localPosition);
        localNormal = vertexBoneWeights.x * (boneMatrices[bone0] * localNormal)
                + vertexBoneWeights.y * (boneMatrices[bone1] * localNormal)
                + vertexBoneWeights.z * (boneMatrices[bone2] * localNormal)
                + vertexBoneWeights.w * (boneMatrices[bone3] * localNormal);
        localTangent = vertexBoneWeights.x * (boneMatrices[bone0] * localTangent)
                + vertexBoneWeights.y * (boneMatrices[bone1] * localTangent)
                + vertexBoneWeights.z * (boneMatrices[bone2] * localTangent)
                + vertexBoneWeights.w * (boneMatrices[bone3] * localTangent);
    }
    fragTexCoord = vertexTexCoord;
    fragLightmapTexCoord =
            vertexTexCoord2 * lightmapScaleBias.xy + lightmapScaleBias.zw;
    fragWorldPosition = (matModel * localPosition).xyz;
    fragWorldNormal = normalize((matNormal * localNormal).xyz);
    fragWorldTangent = normalize((matModel * localTangent).xyz);
    fragTangentSign = vertexTangent.w;
    fragColor = vertexColor;
    gl_Position = mvp * localPosition;
}
