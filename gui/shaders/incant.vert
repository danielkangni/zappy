#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;

void main() {
    vec4 wp   = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal   = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vUV       = vec2(aPos.x + 0.5, aPos.y);
    gl_Position = uProj * uView * wp;
}
