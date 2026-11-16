#version 330 core

layout(location = 0) in vec2 aCorner;

uniform vec3  uCenter;
uniform float uSize;
uniform mat4  uView;
uniform mat4  uProj;

out vec2 vUV;

void main() {
    vec3 world = uCenter + vec3(aCorner.x * uSize, 0.0, aCorner.y * uSize);
    vUV = aCorner;
    gl_Position = uProj * uView * vec4(world, 1.0);
}
