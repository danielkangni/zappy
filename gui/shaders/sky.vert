#version 330 core
layout(location = 0) in vec2 aPos;
out float vY;
out vec2  vNDC;
void main() {
    vY   = aPos.y * 0.5 + 0.5;
    vNDC = aPos;
    gl_Position = vec4(aPos, 0.999, 1.0);
}
