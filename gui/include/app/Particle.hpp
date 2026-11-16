#pragma once
#include <glm/glm.hpp>

struct Spark {
    glm::vec3 pos, vel;
    float life, maxLife, size, phase, twFreq;
    glm::vec3 color;
};
