#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;

uniform vec3  uColor;
uniform float uAlpha;
uniform vec3  uCamPos;
uniform bool  uPillarMode;

uniform vec3  uRingCenter;
uniform float uRingInnerR;
uniform float uRingOuterR;

out vec4 FragColor;

void main() {
    vec3  viewDir = normalize(uCamPos - vWorldPos);
    float cosV    = abs(dot(normalize(vNormal), viewDir));

    float fresnel = 1.0 - cosV;
    fresnel       = fresnel * fresnel * fresnel;

    float intensity;

    if (uPillarMode) {

        float cx   = (vUV.x - 0.5) * 2.0;
        float diff = exp(-cx * cx * 6.0);
        float spec = exp(-cx * cx * 55.0) * 0.60;
        float hFade = pow(max(1.0 - vUV.y, 0.0), 1.4);

        intensity = (diff + spec) * hFade * (0.50 + 0.50 * fresnel);
    } else {

        float r    = length(vWorldPos.xz - uRingCenter.xz);
        float span = max(uRingOuterR - uRingInnerR, 0.001);
        float frac = clamp((r - uRingInnerR) / span, 0.0, 1.0);
        float cx   = (frac - 0.5) * 2.0;
        float radial = exp(-cx * cx * 5.0);

        intensity = radial * (0.45 + 0.35 * fresnel + 0.20);
    }

    FragColor = vec4(uColor, uAlpha * clamp(intensity, 0.0, 1.0));
}
