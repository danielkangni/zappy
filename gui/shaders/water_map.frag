#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;

uniform sampler2D uTex;
uniform float     uTime;
uniform float     uAlpha;
uniform vec2      uSunXZ;
uniform vec3      uSunCol;
uniform vec3      uColor;

out vec4 FragColor;

void main() {
    vec2 uv = vWorldPos.xz * 0.18;
    vec2 a  = uv * 2.5 + vec2(uTime * 0.020, uTime * 0.015);
    vec2 b  = uv * 1.7 - vec2(uTime * 0.017, uTime * 0.012);
    vec3 c  = (texture(uTex, a).rgb + texture(uTex, b).rgb) * 0.5;
    c = mix(c, uColor, 0.45);

    vec2  xz      = normalize(vWorldPos.xz + 1e-4);
    float along   = dot(xz, uSunXZ);
    float streak  = smoothstep(0.45, 1.0, along);
    float shimmer = texture(uTex, uv * 7.0 + uTime * 0.05).r;
    c += uSunCol * streak * (0.35 + 0.65 * shimmer) * 0.9;

    FragColor = vec4(c, uAlpha);
}
