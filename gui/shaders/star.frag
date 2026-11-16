#version 330 core

in vec2 vUV;

uniform vec3  uColor;
uniform float uIntensity;

out vec4 FragColor;

void main() {
    float r = length(vUV);

    float core = exp(-r * r * 11.0);
    float halo = exp(-r * 2.6) * 0.40;

    float sx = pow(max(0.0, 1.0 - abs(vUV.x)), 2.0) * exp(-abs(vUV.y) * 16.0);
    float sy = pow(max(0.0, 1.0 - abs(vUV.y)), 2.0) * exp(-abs(vUV.x) * 16.0);
    float spikes = (sx + sy) * 0.55;

    float i = (core + halo + spikes) * uIntensity;

    i *= 1.0 - smoothstep(0.55, 1.0, r);

    vec3 col = uColor * i + vec3(1.0) * core * uIntensity * 0.8;
    FragColor = vec4(col, 1.0);
}
