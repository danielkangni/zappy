#version 330 core
in vec3 vPos;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uTex;
uniform vec3  uDirLightDir;
uniform vec3  uDirLightColor;
uniform vec3  uPointPos;
uniform vec3  uPointColor;
uniform float uAmbient;
uniform float uAlpha;
uniform float uBright;

out vec4 FragColor;

void main() {
    vec3 base = texture(uTex, vUV).rgb;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uDirLightDir);
    float diff = max(dot(N, L), 0.0);

    vec3 col = base * uAmbient + base * uDirLightColor * diff;

    vec3  PL    = uPointPos - vPos;
    float pdiff = max(dot(N, normalize(PL)), 0.0);
    float atten = 1.0 / (1.0 + 0.08 * dot(PL, PL));
    col += base * uPointColor * pdiff * atten;

    col *= uBright;
    FragColor = vec4(col, uAlpha);
}
