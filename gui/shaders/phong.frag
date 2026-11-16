#version 330 core

in vec3  vPos;
in vec3  vNormal;
in float vWaveH;
in vec3  vVertColor;
in vec2  vTexCoord;

uniform vec3  uCamPos;
uniform vec3  uColor;
uniform vec3  uColor2;
uniform vec3  uColor3;
uniform float uSplit1;
uniform float uSplit2;
uniform bool  uMultiColor;
uniform bool      uUseVertexColor;
uniform sampler2D uTexture;
uniform bool      uUseTexture;
uniform bool      uAlphaCutout;
uniform float uShininess;
uniform float uAmbientStrength;
uniform bool  uEmissive;
uniform bool  uCrystal;
uniform bool  uWave;
uniform float uAlpha;
uniform bool  uCircleClip;
uniform bool  uShoreBlend;
uniform vec2  uClipCenter;
uniform float uClipRadius;
uniform float uShoreInnerR;
uniform float uShoreWidth;

uniform vec3 uDirLightDir;
uniform vec3 uDirLightColor;

uniform vec3 uPointLight0Pos;
uniform vec3 uPointLight0Color;
uniform vec3 uPointLight1Pos;
uniform vec3 uPointLight1Color;

out vec4 FragColor;

void main() {
    if (uEmissive) {
        FragColor = vec4(uColor * 1.4, 1.0);
        return;
    }

    vec3 baseColor = uColor;
    float texAlpha = 1.0;
    if      (uUseTexture) {
        vec4 t = texture(uTexture, vTexCoord);
        baseColor = t.rgb;
        texAlpha  = t.a;
        if (uAlphaCutout && texAlpha < 0.15) discard;
    }
    else if (uUseVertexColor) baseColor = vVertColor;
    if (!uUseTexture && !uUseVertexColor && uMultiColor) {
        if      (vPos.y < uSplit2) baseColor = uColor3;
        else if (vPos.y < uSplit1) baseColor = uColor2;
    }

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uDirLightDir);
    vec3 V = normalize(uCamPos - vPos);

    float diff   = max(dot(N, L), 0.0);
    vec3  diffuse = baseColor * uDirLightColor * diff;
    vec3  ambient = baseColor * uAmbientStrength * vec3(1.10, 0.88, 0.65);
    vec3  col     = ambient + diffuse;

    col = pow(clamp(col, 0.0, 1.0), vec3(0.88));
    col = mix(vec3(dot(col, vec3(0.299, 0.587, 0.114))), col, 1.25);

    if (uCrystal) {

        vec3  H    = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), uShininess);
        vec3 specColor = mix(vec3(1.0), baseColor, 0.25);
        col += specColor * spec * 2.0;

        float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.5);
        col += baseColor * fresnel * 0.9;

        col += baseColor * 0.15;

        FragColor = vec4(clamp(col, 0.0, 1.0), uAlpha);
        return;
    }

    if (uWave) {
        float t = clamp((vWaveH + 0.21) / 0.42, 0.0, 1.0);
        col = mix(col * 0.6, col * 1.2 + vec3(0.04, 0.08, 0.14), t);
    }

    float alpha = uAlpha;
    if (uAlphaCutout) alpha *= texAlpha;
    if (uCircleClip) {
        float d    = length(vPos.xz - uClipCenter);
        float fade = clamp((uClipRadius - d) / (uClipRadius * 0.06), 0.0, 1.0);
        if (fade <= 0.0) discard;
        alpha *= fade;
    }
    if (uShoreBlend) {
        float d = length(vPos.xz - uClipCenter);
        alpha *= smoothstep(uShoreInnerR, uShoreInnerR + uShoreWidth, d);
    }
    FragColor = vec4(clamp(col, 0.0, 1.0), alpha);
}
