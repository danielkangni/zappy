#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aJoints;
layout(location = 3) in vec4 aWeights;
layout(location = 4) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMat;

const int MAX_BONES = 128;
uniform mat4 uBones[128];

out vec3  vPos;
out vec3  vNormal;
out float vWaveH;
out vec3  vVertColor;
out vec2  vTexCoord;

void main() {

    mat4 skin = aWeights.x * uBones[int(aJoints.x)]
              + aWeights.y * uBones[int(aJoints.y)]
              + aWeights.z * uBones[int(aJoints.z)]
              + aWeights.w * uBones[int(aJoints.w)];

    vec4 skinnedPos    = skin * vec4(aPos, 1.0);
    vec4 skinnedNormal = skin * vec4(aNormal, 0.0);

    vec4 worldPos = uModel * skinnedPos;
    vPos    = worldPos.xyz;
    vNormal = normalize(uNormalMat * skinnedNormal.xyz);

    vWaveH     = 0.0;
    vVertColor = vec3(1.0);
    vTexCoord  = aTexCoord;
    gl_Position = uProj * uView * worldPos;
}
