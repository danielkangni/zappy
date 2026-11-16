#version 330 core

layout(location = 0) in vec2 aCorner;

uniform mat4  uView;
uniform mat4  uProj;
uniform float uSize;

out vec2 vUV;
out vec2 vXZ;

void main() {
    vec3 world = vec3(aCorner.x * uSize, 0.0, aCorner.y * uSize);
    vUV = aCorner * 0.5 + 0.5;
    vXZ = aCorner;
    gl_Position = uProj * uView * vec4(world, 1.0);
}
