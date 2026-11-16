#version 330 core
in vec2 vXZ;

uniform vec3  uColor;
uniform vec3  uColorFar;
uniform float uAlpha;

out vec4 FragColor;

void main() {
    float r = length(vXZ);

    float a = (1.0 - smoothstep(0.45, 1.0, r)) * uAlpha;

    float far = smoothstep(0.0, 1.0, clamp(vXZ.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 col = mix(uColor, uColorFar, far);
    FragColor = vec4(col, a);
}
