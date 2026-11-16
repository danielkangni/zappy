#version 330 core

in vec2 vUV;

uniform vec3  uColor;
uniform float uIntensity;

out vec4 FragColor;

void main() {
    float r = length(vUV);
    float a = exp(-r * r * 3.2) * (1.0 - smoothstep(0.65, 1.0, r));
    if (a < 0.04) discard;
    FragColor = vec4(uColor * uIntensity * a, 1.0);
}
