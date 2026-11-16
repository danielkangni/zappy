#version 330 core
in float vY;
in vec2  vNDC;

uniform vec3  uSkyTop;
uniform vec3  uSkyHorizon;
uniform mat4  uInvViewProj;
uniform vec3  uCamPos;
uniform vec3  uSunDir;
uniform vec3  uMoonDir;
uniform float uSunStrength;
uniform float uMoonStrength;

out vec4 FragColor;

float hash(vec3 p) {
    return fract(sin(dot(floor(p), vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

void main() {
    float t = pow(max(vY, 0.0), 0.65);
    vec3 sky = mix(uSkyHorizon, uSkyTop, t);

    vec4 clip  = vec4(vNDC, 1.0, 1.0);
    vec4 world = uInvViewProj * clip;
    world /= world.w;
    vec3 rayDir = normalize(world.xyz - uCamPos);

    float cosA   = dot(rayDir, uSunDir);
    float disc   = smoothstep(0.9985, 0.9998, cosA);
    float corona = smoothstep(0.975,  0.9985, cosA) * 0.55;
    float glow   = smoothstep(0.90,   0.975,  cosA) * 0.35;
    vec3  sunColor  = vec3(1.0, 0.75, 0.20);
    vec3  glowColor = vec3(1.0, 0.45, 0.05);
    sky += glowColor * glow   * uSunStrength;
    sky += glowColor * corona * uSunStrength;
    sky  = mix(sky, sunColor, disc * uSunStrength);

    float cosM  = dot(rayDir, uMoonDir);
    float mglow = smoothstep(0.93, 0.9985, cosM) * 0.28;
    sky += vec3(0.55, 0.62, 0.85) * mglow * uMoonStrength;

    FragColor = vec4(clamp(sky, 0.0, 1.0), 1.0);
}
