#version 330 core
in vec2 vUV;
in vec2 vXZ;

uniform sampler2D uTex;
uniform float uTime;
uniform float uAlpha;
uniform vec2  uSunXZ;
uniform vec3  uSunCol;

out vec4 FragColor;

void main() {

    vec2 a = vUV * 2.5 + vec2(uTime * 0.020, uTime * 0.015);
    vec2 b = vUV * 1.7 - vec2(uTime * 0.017, uTime * 0.012);
    vec3 c = (texture(uTex, a).rgb + texture(uTex, b).rgb) * 0.5;
    c = mix(c, vec3(0.10, 0.30, 0.52), 0.45);

    float along   = dot(normalize(vXZ + 1e-4), uSunXZ);
    float streak  = smoothstep(0.45, 1.0, along);
    float shimmer = texture(uTex, vUV * 7.0 + uTime * 0.05).r;
    c += uSunCol * streak * (0.35 + 0.65 * shimmer) * 0.9;

    float edge = 1.0 - smoothstep(0.55, 1.0, length(vXZ));
    FragColor = vec4(c, uAlpha * edge);
}
