#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aJoints;
layout(location = 3) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

const int MAX_BONES = 128;
uniform mat4 uBones[128];

void main() {
    mat4 skin = aWeights.x * uBones[int(aJoints.x)]
              + aWeights.y * uBones[int(aJoints.y)]
              + aWeights.z * uBones[int(aJoints.z)]
              + aWeights.w * uBones[int(aJoints.w)];

    gl_Position = uProj * uView * uModel * skin * vec4(aPos, 1.0);
}
